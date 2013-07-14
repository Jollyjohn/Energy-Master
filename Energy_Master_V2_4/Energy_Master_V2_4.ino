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

// Pin for the LCD backlight switch
#define LCDBACKLIGHT 13

// This is the INT0 Pin of the Etherten
#define INMETERPIN  2

// This is the INT1 Pin of the Etherten
#define OUTMETERPIN 3

// char array for constructing the COSM data strings so we can report it
char COSM_inMeter[12];
char COSM_inCumulative[12];
char COSM_outMeter[12];
char COSM_outCumulative[12];
char COSM_diff[12];

char inEnergyDisp[7];
char outEnergyDisp[7];

// Ethernet parameters for the main network
byte my_mac[]     = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x22 };   // Our MAC address
byte my_ip[]      = { 192, 168, 100, 22 };                    // Our IP address
byte net_router[] = { 192, 168, 100, 1  };                    // The network router facing the internet
byte net_dns[]    = { 192, 168, 100, 9  };                    // The network DNS server

EthernetClient client;                                 // We are a client meaning we call them, we don't look for connections

// initialize an LCD display instance with the numbers of the interface pins
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// We need to declare the data exchange
// variable to be volatile - the value is
// read from memory.
volatile int inMeterPulses  = 0;
volatile int outMeterPulses = 0;

// These keep track of the cumulative totals
long inMeterCumulative;
long outMeterCumulative;

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
  pinMode(INMETERPIN, INPUT);
  pinMode(OUTMETERPIN, INPUT);
  
  // Attach the interupt handler for the Main Meter
  lcd.setCursor(0,2);
  lcd.print(" - Interupts -  ");
  
  attachInterrupt(0,inCountPulse,FALLING);
  attachInterrupt(1,outCountPulse, FALLING);
    
  // start the Ethernet connection and the server:
  lcd.setCursor(0,2);
  lcd.print(" -  Network  - ");
  Ethernet.begin(my_mac, my_ip, net_dns, net_router);

  // Here we should reach out to COSM and get the last
  // value for the cumulative totals so we can keep adding to them
  // For now we just start from scratch
  inMeterCumulative = 0;      // Should come from http://api.cosm.com/v2/feeds/42622/datastreams/3.csv

//
//  if (client.connect("api.cosm.com", 80)) {
//      
//    client.println("GET /v2/feeds/42622datastream/3.csv");                        // Send the HTTP GET request.
//    
//    if (client.available()) {   // keep reading the return data till COSM stops sending it
//      char c = client.read();   // Just throw it away. If it worked good, if not, too bad.
//    }
//  
//

  outMeterCumulative = 0;     // Should come from http://api.cosm.com/v2/feeds/42622/datastreams/4.csv
  
  
  lcd.setCursor(0,2);
  lcd.print(" -    Done    - ");
  
  delay(2000);
  
  lcd.clear();
  lcd.print("In:   |Out:  | ");
  lcd.setCursor(0,2);
  lcd.print("-----W|-----W| ");
}

void loop() {
  // Clear the calculated energy for both meters
  int inEnergy  = 0;   // Energy taken in from the electricity company
  int outEnergy = 0;   // Energy sent out to the electricty company
  
  // Reset the pulse count on both meters
  inMeterPulses  = 0;
  outMeterPulses = 0;
  
  // Ensure interrupts are on
  interrupts();
  
  // Count meter pulses for 30 seconds (30s * 1000ms/s) = 30,000ms
  delay(30000);
  
  // Stop counting
  noInterrupts();
  
  
  // Main Meter
  // Each pulse is 1.25Wh of energy and we count the number of pulses over 30 seconds. This gives us Wh/30s
  // To convert it to Wh/h we need to extrapolate over 60 minutes. This is done by multiplying by (60m/30s = 3600s/30s = 120). That will give Wh/h, which is W.
  // If we precalculate the (1.25 * 120) we get 150. This allows us to stick to integer maths.
  inEnergy = inMeterPulses * 150;
  inMeterCumulative = inMeterCumulative + long(inMeterPulses * 1.25);
 
  // Create power consumed string to be sent to Cosm
  sprintf(COSM_inMeter,"0,%d",inEnergy);                        // This data goes into stream 0 on COSM
  sprintf(COSM_inCumulative, "3,%lu", inMeterCumulative);       // This data goes onto stream 3 on COSM
  
  
  // Solar Meter
  // Each pulse is 1Wh of energy and we count the number of pulses over 30 seconds. This gives Wh/30s
  // To convert it to Wh/h we need to extrapolate over 60 minutes. This is done by multiplying by (60m/30s = 3600s/30s = 120). That will give Wh/h, which is W.
  // If we precalculate the (1 * 12) we get 12. This allows us to stick to integer maths.
  outEnergy = outMeterPulses * 120;
  outMeterCumulative = outMeterCumulative + outMeterPulses;
 
  // Create power produced string to be sent to Cosm
  sprintf(COSM_outMeter,"1,%d",outEnergy);                      // This data goes into stream 1 on COSM
  sprintf(COSM_outCumulative,"4,%lu", outMeterCumulative);      // This data goes into stream 4 on COSM
  
  
  // Create power consumption difference string to be sent to Cosm
  sprintf(COSM_diff,"2,%d",(outEnergy-inEnergy));            // This data goes into stream 2 on COSM
  
  
  // Update the LCD display
  sprintf(inEnergyDisp,"%5dW|",inEnergy);
  lcd.setCursor(0,2);
  lcd.print(inEnergyDisp);
  
  sprintf(outEnergyDisp,"%5dW",outEnergy);
  lcd.setCursor(7,2);
  lcd.print(outEnergyDisp);

  //Report it all via COSM
  if (client.connect("api.cosm.com", 80)) {
    
    delay(200);      // Take this out and it stops working - why?
    
    client.println("PUT /v2/feeds/42622.csv HTTP/1.1");                        // Send the HTTP PUT request.
    client.println("Host: api.cosm.com");                                      // Send the host command        
    client.println("X-ApiKey: y_eXWNhsWfsaedd6VhbA13e9qVYCa1_ck5VniQ-3uUw");   // Prove we are authorised to update this feed        
    client.println("User-Agent: Energy-Master");                               // Explain who we are     
    client.print  ("Content-Length: ");
       client.println(strlen(COSM_inMeter)+2+
                      strlen(COSM_outMeter)+2+
                      strlen(COSM_diff)+2+
                      strlen(COSM_inCumulative)+2+
                      strlen(COSM_outCumulative));                             // Send the length of the data being sent (remember the CR/LF)
    client.println("Content-Type: text/csv");                                  // Content type      
    client.println("Connection: close");                                       // Explain we will close the connection 
    client.println();                                                          // Don't forgte the empty line between the header and the data!!
    client.println(COSM_inMeter);                                              // Send the data
    client.println(COSM_outMeter);
    client.println(COSM_diff);
    client.println(COSM_inCumulative);
    client.println(COSM_outCumulative);
    client.println();

    if (client.available()) {   // keep reading the return data till COSM stops sending it
      char c = client.read();   // Just throw it away. If it worked good, if not, too bad.
    }
    
    client.flush();
    client.stop();
  }    
}
