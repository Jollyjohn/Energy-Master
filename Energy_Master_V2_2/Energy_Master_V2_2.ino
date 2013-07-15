/******************************************************************
*******************************************************************
** Houshold Energy Management
**
** The prupose of this controller is to monitor the enery consumption of the house.
** The try and get as close as possible to the "offical" readings this system
** monitors the pulses on the suppliers enegry meters.
**
*/

#include <SPI.h>
#include <Ethernet.h>
#include <LiquidCrystal.h>
#include <HttpClient.h>
#include <Cosm.h>

#define LCDBACKLIGHT 13       // Pin for the LCD backlight switch (Need to change to support alternate int channel)

// This is the INT0 Pin of the ATMega8
#define INMETERPIN  2
#define OUTMETERPIN 3

// char array for constructing the COSM data strings so we can report it
char COSM_inMeter[12];
char COSM_outMeter[12];
char COSM_diff[12];

char inEnergyDisp[7];
char outEnergyDisp[7];

// Ethernet parameters for the main network
byte my_mac[]   = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x22 };   // Our MAC address
byte my_ip[]    = { 192, 168, 100, 22 };                    // Our IP address
byte router[]   = { 192, 168, 100, 1  };                    // The network router facing the internet
byte dns_serv[] = { 192, 168, 100, 9  };                    // The network DNS server

EthernetClient client;                                 // We are a client meaning we call them, we don't look for connections

// initialize an LCD display instance with the numbers of the interface pins
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// We need to declare the data exchange
// variable to be volatile - the value is
// read from memory.
volatile int inMeterPulses  = 0;
volatile int outMeterPulses = 0;

// Install the interrupt routine for the Main Energy Meter
//
// This counts the number of pulses we have seen from the main energy meeter.
// Each pulse counts for 1.25Wh of energy

void inCountPulse() {
  inMeterPulses++;    // Count the number of pulses that are happening on meter channel 1
}

// This counts the number of pulses we have seen from the solar energy meeter.
// Each pulse counts for 1Wh of energy

void outCountPulse() {
  outMeterPulses++;    // Count the number of pulses that are happening on meter channel 2
}

// Set the Arduino and associated peripherals
void setup() {

  // set up the LCD
  pinMode(LCDBACKLIGHT, OUTPUT);           // LCD backlight
  digitalWrite(LCDBACKLIGHT, HIGH);        // Turn on the backlight
  
  lcd.begin(16,2);                         // number of columns and rows: 
  lcd.clear();                             // Clear the display
  lcd.print(" Energy Master  ");           // Who are we?
  
  lcd.setCursor(0,2);
  lcd.print(" -Initialising- ");           // Display startup message
  
  // Sets the input pin as input with a 10k resistor to VCC. The sensor will pull this low at each light pulse
  pinMode(INMETERPIN, INPUT_PULLUP);
  pinMode(OUTMETERPIN, INPUT_PULLUP);
  
  // Attach the interupt handler for the Main Meter
  lcd.setCursor(0,2);
  lcd.print(" - Interupts -  ");
  
  attachInterrupt(0,inCountPulse,FALLING);
  attachInterrupt(1,outCountPulse, FALLING);
    
  // start the Ethernet connection and the server:
  lcd.setCursor(0,2);
  lcd.print(" -  Network  - ");
  Ethernet.begin(my_mac, my_ip, dns_serv, router);

  lcd.setCursor(0,2);
  lcd.print(" -    Done    - ");
  
  delay(2000);
  
  lcd.clear();
  lcd.print("In:    |Out:   ");
  lcd.setCursor(0,2);
  lcd.print("-----W |-----W ");
}

void loop() {
  // Clear the calculated energy for both meters
  int inEnergyUsed = 0;
  int outEnergyUsed = 0;
  
  // Reset the pulse count on both meters
  inMeterPulses = 0;
  outMeterPulses = 0;
  
  // Ensure interrupts are on
  interrupts();
  
  // Count pulses for 5 minutes (5m * 60s/m * 1000ms/s) = 300,000ms
  delay(30000);
  
  // Stop counting
  noInterrupts();
  
  // Main Meter
  // Each pulse is 1.25Wh of energy and we count the number of pulses over 5 minutes. To convert it to kW we need to extrapolate over 60 minutes (multiply by 12)
  // That will give Wh/h, which is W. If we precalculate the (1.25 * 12) we get 15. This allows us to stick to integer maths.
  inEnergyUsed = inMeterPulses * 150;
 
  // Create power in string to be sent to Cosm
  sprintf(COSM_inMeter,"0,%d",inEnergyUsed);        // This data goes into stream 0 on COSM
  
  // Solar Meter
  // Each pulse is 1Wh of energy and we count the number of pulses over 5 minutes. To convert it to kW we need to extrapolate over 60 minutes (multiply by 12)
  // That will give Wh/h, which is W. If we precalculate the (1 * 12) we get 12. This allows us to stick to integer maths.
  outEnergyUsed = outMeterPulses * 120;
 
  // Create power in string to be sent to Cosm
  sprintf(COSM_outMeter,"1,%d",outEnergyUsed);        // This data goes into stream 0 on COSM
  
  sprintf(COSM_diff,"2,%d",(outEnergyUsed-inEnergyUsed));
  
  // Update the LCD display
  sprintf(inEnergyDisp,"%5d",inEnergyUsed);
  lcd.setCursor(0,2);
  lcd.print(inEnergyDisp);
  
  sprintf(outEnergyDisp,"%5d",outEnergyUsed);
  lcd.setCursor(8,2);
  lcd.print(outEnergyDisp);

  //Report it all via COSM
  if (client.connect("api.cosm.com", 80)) {
      delay(100);                                                                   // (Test 3) Wait for COSM to be ready
      
      client.println("PUT /v2/feeds/42622.csv HTTP/1.1");                           // Send the HTTP PUT request.
      client.println("Host: api.cosm.com");                                         // Send the host command        
      client.println("X-ApiKey: y_eXWNhsWfsaedd6VhbA13e9qVYCa1_ck5VniQ-3uUw");      // Prove we are authorised to update this feed        
      client.println("User-Agent: Energy-Master");                                  // Explain who we are     
      client.print  ("Content-Length: ");
         client.println((strlen(COSM_inMeter)+strlen(COSM_outMeter)+2+strlen(COSM_diff)+2));              // Send the length of the data being sent        
      client.println("Content-Type: text/csv");                                     // Content type      
      client.println("Connection: close");                                          // Explain we will close the connection 
      client.println();                                                             // Don't forgte the empty line between the header and the data!!
      client.println(COSM_inMeter);                                                 // Send the data
      client.println(COSM_outMeter);
      client.println(COSM_diff);
      client.println();

      while (client.available()) {
          client.read();                                                            // Gather the returned result
      }   
  }

    delay(100);
   
    if (client.connected()) {
      client.stop();
    }
    
    Ethernet.begin(my_mac, my_ip, dns_serv, router);
}
