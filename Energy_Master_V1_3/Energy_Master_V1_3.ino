/******************************************************************
*******************************************************************
** Houshold Energy Management
**
** The prupose of this controller is to monitor the enery consumption of the house.
** The try and get as close as possible to the "offical" readings this system
** monitors the pulses on the suppliers enegry meter.
**
*/

#include <SPI.h>
#include <Ethernet.h>
#include <LiquidCrystal.h>
#include <avr/pgmspace.h>

#define LCDBACKLIGHT   3       // Pin for the LCD backlight switch (Need to change to support alternate int channel)

// This is the INT0 Pin of the ATMega8
#define SENSEPIN 2

// COSM data
#define COSM_PORT 80                        // Normally 80; can be changed for debugging

// char array for constructing the COSM data string so we can measure it
char COSM_data[12];

// Ethernet parameters for the main network
byte my_mac[]   = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x22 };   // Our MAC address
byte my_ip[]    = { 192, 168, 100, 22 };                    // Our IP address
byte net_router[]   = { 192, 168, 100, 1 };                     // The network router facing the internet
byte net_dns[] = {192, 168, 100, 9 };                      // The network DNS server

EthernetClient client;                                 // We are a client meaning we call them, we don't look for connections

// initialize an LCD display instance with the numbers of the interface pins
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// We need to declare the data exchange
// variable to be volatile - the value is
// read from memory.
volatile int meterPulses = 0;

// Install the interrupt routine for the Main Energy Meter
//
// This counts the number of pulses we have seen from the main energy meeter.
// Each pulse counts for 1.25Wh of energy

void countPulse() {
  meterPulses++;    // Count the number of pulses that are happening on meter channel 1
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
  
  // sets the input pin as input with a 10k resistor to VCC. The sensor will pull this low at each light pulse
  pinMode(SENSEPIN, INPUT_PULLUP);
  
  // Attach the interupt handler for the Main Meter
  lcd.setCursor(0,2);
  lcd.print(" - Interupts -  ");
  attachInterrupt(0,countPulse,RISING);
    
  // start the Ethernet connection and the server:
  lcd.setCursor(0,2);
  lcd.print(" -  Network  - ");
  Ethernet.begin(my_mac, my_ip, net_dns, net_router);

  lcd.setCursor(0,2);
  lcd.print(" -    Done    - ");
}

void loop() {
  // Clear the calculated energy
  int energyUsed = 0;
  
  // Reset the pulse count
  meterPulses = 0;
  
  // Ensure interrupts are on
  interrupts();
  
  // Count pulses for 5 minutes (5m * 60s/m * 1000ms/s) = 300,000ms
  delay(300000);
  
  // Stop counting
  noInterrupts();
  
  // Each pulse is 1.25Wh of energy over 5 minutes. To convert it to kW we need to extrapolate over 60 minutes (multiply by 12)
  // That will give Wh/h, which is W. If we precalculate the (1.25 * 12) we get 15. This allows us to stick to integer maths.
  energyUsed = meterPulses * 15;
 
  // Create string to be sent to Cosm
  sprintf(COSM_data,"0,%d",energyUsed);        // This data goes into stream 0 on COSM
  
  lcd.setCursor(0,2);
  lcd.print("Using ");
  lcd.print(energyUsed);
  lcd.print("kWh   ");

  //Report it all via COSM
  if (client.connect("api.cosm.com", COSM_PORT)) {
      delay(100);                    // (Test 3) Wait for COSM to be ready
      
      client.println(PSTR("PUT /v2/feeds/42622.csv HTTP/1.1\nHost: api.cosm.com\nX-ApiKey: y_eXWNhsWfsaedd6VhbA13e9qVYCa1_ck5VniQ-3uUw\nUser-Agent: Energy-Master\nContent-Length: "));
      client.println(strlen(COSM_data));        // Send the length of the data being sent        
      client.println(PSTR("Content-Type: text/csv\nConnection: close"));                                          // Explain we will close the connection 
      client.println();                                                             // Don't forgte the empty line between the header and the data!!
      client.println(COSM_data);                                                    // Send the data
      
  }

    delay(1000);
   
    client.flush();  // Discard anything that was sent back to us.
    
    if (client.connected()) {
      client.stop();
    }
}
