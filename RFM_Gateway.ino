/*
Author:  Eric Tsai
License:  CC-BY-SA, https://creativecommons.org/licenses/by-sa/2.0/
Date:  7-21-2014
File: RFM_Gateway.ino
This sketch receives RFM wireless data and forwards it to I2C

Modifications Needed:
1)  Update encryption string "ENCRYPTKEY"
2)  
*/

/*
RFM69 Pinout:
    MOSI = 11
    MISO = 12
    SCK = 13
    SS = 10
*/


//general --------------------------------
#define SERIAL_BAUD   9600



//RFM69  ----------------------------------
#include <RFM69.h>
#include <SPI.h>
#define NODEID        1    //unique for each node on same network
#define NETWORKID     101  //the same on all nodes that talk to each other
#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "xxxxxxxxxxxxxxxx" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ACK_TIME      30 // max # of ms to wait for an ack

RFM69 radio;
bool promiscuousMode = false; //set to 'true' to sniff all packets on the same network
//I2C  -----------------------------------
#include <Wire.h>
const byte TARGET_ADDRESS = 42;

typedef struct {		
  int                   nodeID; 
  int			sensorID;
  unsigned long         var1_usl; 
  float                 var2_float; 
  float			var3_float;	
} Payload;
Payload theData;

typedef struct {		
  int                   nodeID; 
  int			sensorID;	
  unsigned long         var1_usl;
  float                 var2_float; 
  float			var3_float;
  int                   var4_int;
} itoc_Send;
itoc_Send theDataI2C;


void setup() 
{
  Wire.begin ();
  
  Serial.begin(9600); 

  //RFM69 ---------------------------
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  #ifdef IS_RFM69HW
    radio.setHighPower(); //uncomment only for RFM69HW!
  #endif
  radio.encrypt(ENCRYPTKEY);
  radio.promiscuous(promiscuousMode);
  char buff[50];
  sprintf(buff, "\nListening at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);

}  // end of setup

byte ackCount=0;


void loop() 
{
  
  if (radio.receiveDone())
  {
    //Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
    if (promiscuousMode)
    {
      Serial.print("to [");Serial.print(radio.TARGETID, DEC);Serial.print("] ");
    }
	

    if (radio.DATALEN != sizeof(Payload))
      Serial.println("Invalid payload received, not matching Payload struct!");
    else
    {
      theData = *(Payload*)radio.DATA; //assume radio.DATA actually contains our struct and not something else
      
      Serial.print(theData.sensorID);
      Serial.print(", ");
      Serial.print(theData.var1_usl);
      Serial.print(", ");
      Serial.print(theData.var2_float);
      Serial.print(", ");
      //Serial.print(" var2(temperature)=");
      //Serial.print(", ");
      //Serial.print(theData.var3_float);
      
      //printFloat(theData.var2_float, 5); Serial.print(", "); printFloat(theData.var3_float, 5);
      
      Serial.print(", RSSI= ");
      Serial.println(radio.RSSI);
      
      //save it for i2c:
      theDataI2C.nodeID = theData.nodeID;
      theDataI2C.sensorID = theData.sensorID;
      theDataI2C.var1_usl = theData.var1_usl;
      theDataI2C.var2_float = theData.var2_float;
      theDataI2C.var3_float = theData.var3_float;
      theDataI2C.var4_int = radio.RSSI;      
    }
	
	
    if (radio.ACK_REQUESTED)
    {
      byte theNodeID = radio.SENDERID;
      radio.sendACK();
      //Serial.print(" - ACK sent.");

      // When a node requests an ACK, respond to the ACK
      // and also send a packet requesting an ACK (every 3rd one only)
      // This way both TX/RX NODE functions are tested on 1 end at the GATEWAY
      if (ackCount++%3==0)
      {
        //Serial.print(" Pinging node ");
        //Serial.print(theNodeID);
        //Serial.print(" - ACK...");
        //delay(3); //need this when sending right after reception .. ?
        //if (radio.sendWithRetry(theNodeID, "ACK TEST", 8, 0))  // 0 = only 1 attempt, no retries
        //  Serial.print("ok!");
        //else Serial.print("nothing");
      }
    }//end if radio.ACK_REQESTED
    
    
    //send wireless data to I2C
    Wire.beginTransmission (TARGET_ADDRESS);
    Wire.write ((byte *) &theDataI2C, sizeof theDataI2C);
    Wire.endTransmission ();
    
    
  } //end if radio.receive
  
}//end loop
