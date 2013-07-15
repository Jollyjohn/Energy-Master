/******************************************************************
*******************************************************************
** Houshold Energy Management
**
** The prupose of this controller is to monitor the enery consumption of the house.
** The try and get as close as possible to the "offical" readings this system
** monitors the pulses on the suppliers enegry meter.
**
*/

#include <stdio.h>
#include <stdlib.h>


#define LCDBACKLIGHT   3       // Pin for the LCD backlight switch
// LED connected to digital pin 13
#define LEDPIN 13
// This is the INT0 Pin of the ATMega8
#define SENSEPIN 2

// COSM data
#define COSM_FEED "42622"
#define COSM_KEY  "y_eXWNhsWfsaedd6VhbA13e9qVYCa1_ck5VniQ-3uUw"
//byte COSM[] = { 173, 203, 98, 29 };    // New Pachube address which doesn't seem to work
//byte COSM[] = { 209, 40, 205, 190 };   // Old Pachube address
//byte COSM[] = { 216, 52, 233, 122 };   // From the COSM examples
//byte COSM[] = { 64,  94,  18, 122 };   // From DNS for api.cosm.com
//byte COSM[] = { 64,  94,  18, 121 };   // From DNS for api.pachube.com
//byte COSM[] = { 192, 168, 100, 25 };     // Local test address for debugging
int COSM_port = 80;                    // Normally 80; can be changed for debugging
char COSM_data[10];

#include <SPI.h>
#include <Ethernet.h>

byte my_mac[]   = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x22 };   // Our MAC address
byte my_ip[]    = { 192, 168, 100, 22 };                    // Our IP address
byte router[]   = { 192, 168, 100, 1 };                     // The network router facing the internet
byte dns_serv[] = {192, 168, 100, 9 };                      // The network DNS server

EthernetClient client;                                 // We are a client meaning we call them, we don't look for connections

// Definitions for LCD routines
#include <LiquidCrystal.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// We need to declare the data exchange
// variable to be volatile - the value is
// read from memory.
volatile int meterPulses = 0;

// Install the interrupt routine.
// Main Energy Meter
//
// This counts the number of pulses we have seen from the main energy meeter.
// Each pulse counts for 1.25Wh of energy


void countPulse() {
  // check the value again - since it takes some time to
  // activate the interrupt routine, we get a clear signal.
  meterPulses++;
}


void setup() {

  // set up the LCD
  pinMode(LCDBACKLIGHT, OUTPUT);           // LCD backlight
  digitalWrite(LCDBACKLIGHT, HIGH);        // Turn on the backlight
  
  lcd.begin(16,2);                         // number of columns and rows: 
  lcd.clear();                             // Clear the display
  lcd.print(" Energy Master  ");           // Who are we?
  
  lcd.setCursor(0,2);
  lcd.print(" -Initialising- ");           // Display startup message

  // sets the digital pin as output
  pinMode(LEDPIN, OUTPUT);
  // read from the sense pin
  pinMode(SENSEPIN, INPUT_PULLUP);
  

  // Attach the interupt handler for the Main Meter
  lcd.setCursor(0,2);
  lcd.print(" - Interupts -  ");
  attachInterrupt(0,countPulse,RISING);
  
    
  // start the Ethernet connection and the server:
  lcd.setCursor(0,2);
  lcd.print(" -  Network  - ");
  Ethernet.begin(my_mac, my_ip, dns_serv);
  
  lcd.setCursor(0,2);
  lcd.print(" -    Done    - ");
}

void loop() {
  // Clear the calculated energy
  int energyUsed = 0;
  
  // Reset the pulse count
  meterPulses = 0;
  
  // Count pulses for 5 minutes (5m * 60s/m * 1000ms/s) = 300,000ms
  delay(300000);
  
  // Each pulse is 1.25Wh of energy over 5 minutes. To convert it to kW we need to extrapolate over 60 minutes (multiply by 12)
  // That will give Wh/h, which is W. If we precalculate the (1.25 * 12) we get 15. This allows us to stick to integer maths.
  energyUsed = meterPulses * 15;
 
  // Create string to be sent to Pachube
  sprintf(COSM_data,"0,%d",energyUsed);        // This data goes into stream 0 on COSM
//  dtostrf(energyUsed,6,0,&COSM_data[2]);     // Write this data into the data string, starting at the 3rd character. Make it 6 chars in total with precission of 0 => XXXXX   
  
  lcd.setCursor(0,2);
//  lcd.print(COSM_data);
//  lcd.print(":");
//  lcd.println(strlen(COSM_data)+1);
  lcd.print("Using ");
  lcd.print(energyUsed);
  lcd.print("kWh   ");

  //Report it all via COSM
  if (client.connect("api.cosm.com", COSM_port)) {
      
      // Send the HTTP PUT request. This is a Pachube V2 format request. The .csv tells COSM what format we are using 
      client.print("PUT /v2/feeds/");
      client.print(COSM_FEED);
      client.println(".csv HTTP/1.1");
    
      // Send the host command
      client.println("Host: api.cosm.com");
    
      // Prove we are authorised to update this feed
      client.print("X-PachubeApiKey: ");
      client.println(COSM_KEY);

      // Send the length of the string being sent
      client.print("Content-Length: ");
      client.println((strlen(COSM_data)));
      
      // last pieces of the HTTP PUT request:
      client.println("Connection: close");
    
      // Don't forgte the empty line between the header and the data!!
      client.println();
      

      // Create string to be sent to Pachube
//      sprintf(COSM_data,"0,");                   // This data goes into stream 0 on COSM
//      dtostrf(energyUsed,6,0,&COSM_data[2]);     // Write this data into the data string, starting at the 3rd character. Make it 6 chars in total with precission of 0 => XXXXX   
          
      //  send the data
 //     client.print("0,");
 //     client.println(energyUsed);
      client.println(COSM_data);
      
  }

  client.stop();

}
