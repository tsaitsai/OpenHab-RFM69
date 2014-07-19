/*
Author:  Eric Tsai
License:  CC-BY-SA, https://creativecommons.org/licenses/by-sa/2.0/
Date:  7-21-2014
File: Mailbox.ino
This sleeps until interrupted, then sends data via RFM69

Modifications Needed:
1)  Update encryption string "ENCRYPTKEY"
2)  
*/

//RFM69  --------------------------------------------------------------------------------------------------
#include <RFM69.h>
#include <SPI.h>
#define NODEID        41    //unique for each node on same network
#define NETWORKID     101  //the same on all nodes that talk to each other
#define GATEWAYID     1
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
//#define FREQUENCY   RF69_433MHZ
//#define FREQUENCY   RF69_868MHZ
#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "xxxxxxxxxxxxxxxx" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ACK_TIME      30 // max # of ms to wait for an ack

#define SERIAL_BAUD   9600  //must be 9600 for GPS, use whatever if no GPS
//deviceID's

typedef struct {		
  int           nodeID; 
  int			deviceID;
  unsigned long var1_usl;
  float         var2_float;
  float			var3_float;
} Payload;
Payload theData;

char buff[20];
byte sendSize=0;
boolean requestACK = false;
RFM69 radio;

//end RFM69 ------------------------------------------


//analog read
int analogPin = 0;
byte adcsra_save;

//-------------------------
#include <avr/sleep.h>

const byte LED = 8;
int scan_num = 0;		//keep track of how many times opened.
void wake ()
{
  // must do this as the pin will probably stay low for a while
  detachInterrupt (1);
  // cancel sleep as a precaution
  sleep_disable();

}  // end of wake

void setup () 
  {
  digitalWrite (2, HIGH);  // enable pull-up
  Serial.begin(SERIAL_BAUD);  //Begin serial communcation
  
  adcsra_save = ADCSRA;	//save ADCSRA to re-enable later.
  
  Serial.println("starting");
  //RFM69-------------------------------------------
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  #ifdef IS_RFM69HW
    radio.setHighPower(); //uncomment only for RFM69HW!
  #endif
  radio.encrypt(ENCRYPTKEY);
  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  theData.nodeID = 41;  //this node id should be the same for all devices in this node
  pinMode (LED, OUTPUT);
  Serial.println("finished setup");
  }  // end of setup

void loop () 
{
 

  digitalWrite (LED, HIGH);
  
  //re-enable analog:
  ADCSRA = adcsra_save;
  
  scan_num = scan_num + 1;
  //send data
  theData.nodeID = 41; 
  theData.deviceID = 2;
  theData.var1_usl = millis();
  theData.var2_float = scan_num;
  
  //radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData));
  delay(500);
  theData.var3_float = (analogRead(A3))*3.30/1023.00*2.00;
  
  //use either send;  sendwithretry sends it 3 times if no ack
  //radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData));
  radio.send(GATEWAYID, (const void*)(&theData), sizeof(theData));
  
  digitalWrite (LED, LOW);

  radio.sleep();
  // disable ADC
  ADCSRA = 0;  
  
  set_sleep_mode (SLEEP_MODE_PWR_DOWN);  
  sleep_enable();

  // Do not interrupt before we go to sleep, or the
  // ISR will detach interrupts and we won't wake.
  noInterrupts ();
  
  // will be called when pin D2 goes HIGH  
  attachInterrupt (1, wake, HIGH);
 
  // turn off brown-out enable in software
  // BODS must be set to one and BODSE must be set to zero within four clock cycles
  MCUCR = bit (BODS) | bit (BODSE);
  // The BODS bit is automatically cleared after three clock cycles
  MCUCR = bit (BODS); 
  
  // We are guaranteed that the sleep_cpu call will be done
  // as the processor executes the next instruction after
  // interrupts are turned on.
  interrupts ();  // one cycle
  sleep_cpu ();   // one cycle

  } // end of loop
