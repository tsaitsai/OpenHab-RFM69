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
#define NODEID        40    //unique for each node on same network
#define NETWORKID     10  //the same on all nodes that talk to each other
#define GATEWAYID     1
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
//#define FREQUENCY   RF69_433MHZ
//#define FREQUENCY   RF69_868MHZ
#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "XXXXXXXXXXXXXXXX" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ACK_TIME      30 // max # of ms to wait for an ack
#define LED           9  // Moteinos have LEDs on D9
#define SERIAL_BAUD   9600  //must be 9600 for GPS, use whatever if no GPS
//deviceID's
// 2 = light sensor
// 3 = ultrasonic
// 4 = temperature_F/humidity
// 5 = presence sensor PIR

typedef struct {		
  int           nodeID; 		//node ID (1xx, 2xx, 3xx);  1xx = basement, 2xx = main floor, 3xx = outside
  int			deviceID;		//sensor ID (2, 3, 4, 5)
  unsigned long var1_usl; 		//uptime in ms
  float         var2_float;   	//sensor data?
  float			var3_float;		//battery condition?
} Payload;
Payload theData;

char buff[20];
byte sendSize=0;
boolean requestACK = false;
RFM69 radio;

//end RFM69 ------------------------------------------



//device DHT11 Temperature/Humidity
#include <DHT.h>
#define DHTPIN 4     // what pin we're connected to
#define DHTTYPE DHT11   // DHT 11 
DHT dht(DHTPIN, DHTTYPE);


//device light sensor
int lightPin = 0;  //define an analog pin for Photo resistor


//device ultrasonic
const int pingPin = 3;
const int trigPin = 5;

//device PIR
const int PIRpin = 6;

//round robbin
int dev2_trans_period = 7000;
long dev2_last_period = -1;
int dev3_trans_period = 5000;
long dev3_last_period = -1;
int dev4_trans_period = 9000;
long dev4_last_period = -1;
int dev5_trans_period = 11000;
long dev5_last_period = -1;




  //time:
  // 2 = light sensor
	const unsigned long dev2_period = 600000;			//send data every X seconds
	const unsigned long dev2_change = 20000;			//examine for change every X seconds
	unsigned long dev2_period_time;	//seconds since last period
	unsigned long dev2_change_time;	//seconds since last examination
	const int dev2_change_amt = 100;			//change amount to notify of change
	int dev2_current_value = -200;			//light sensor current value
  
  // 3 = ultrasonic
	const unsigned long dev3_period = 700000;  //700000			//send data every X seconds
	const unsigned long dev3_change = 15000;			//examine for change every X seconds
	unsigned long dev3_period_time;	//seconds since last period
	unsigned long dev3_change_time;	//seconds since last examination
	const int dev3_change_amt = 12;			//change amount to notify of change
	int dev3_current_value = -200;			//light sensor current value

  // 3 = temperature
	const unsigned long dev4_period = 710000;  //700000			//send data every X seconds
	unsigned long dev4_period_time;	//seconds since last period


  
  
	// 4 = temperature_F/humidity
	// 5 = presence sensor PIR



void setup()
{
  Serial.begin(SERIAL_BAUD);  //Begin serial communcation
  
  //RFM69-------------------------------------------
  
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  #ifdef IS_RFM69HW
    radio.setHighPower(); //uncomment only for RFM69HW!
  #endif
  radio.encrypt(ENCRYPTKEY);
  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  theData.nodeID = 40;  //this node id should be the same for all devices in this node
  
  //--------------------------------------
  //light sensor
  int lightOutput;
  
  
  //device ultrasonic
  pinMode(pingPin, INPUT);
  pinMode(trigPin, OUTPUT);
  
  //device DHT
  dht.begin();
  
  //device PIR
  pinMode(PIRpin, INPUT);
  
  //time:
	//dev2 is light
	dev2_period_time = millis();	//seconds since last period
	dev2_change_time = millis();	//seconds since last examination
	//dev2 is ultrasonic
	dev3_period_time = millis();	//seconds since last period
	dev3_change_time = millis();	//seconds since last examination


	// dev4 is temperature_F/humidity
	dev4_period_time = millis();	//seconds since last period
	
	// 5 = presence sensor PIR
  
}
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
void loop()
{
  
  //---- this is what data looks like -----
  /*
  	theData.nodeID = 40;
	theData.sensorID = xx;  
                                      2	= lights sensor	var1 = 0-1024, higher means more light
                                      3	= ultrasonic	var1 = inches
                                      4	= temperature_F/humidity	var1= degree F, var2=% humidity
                                      5	= presence sensor	var: = 1 = presence, 0=absence
	theData.var1_usl = millis();
        theData.var2_float = ;
        theData.var3_float = ;
  */


  
  
  //---------------- start devices ------------------------------------------------
  
  
  delay(2000); //short delay for faster response to light.
  
  
  //-------------------------------------------------------------------------------
  // deviceID = 2;  //light sensor
  //-------------------------------------------------------------------------------
  //light sensor---------------------
  int lightOutput;

  //always report after X seconds
  unsigned long timepassed = millis() - dev2_period_time;
  
  /*
  Serial.print("    timepassed = ");
  Serial.print(timepassed);
    Serial.print("    dev2_period(60sec) = ");
  Serial.print(dev2_period);
  Serial.print("    millis = ");
  Serial.print(millis());
  Serial.print("    dev2_period_time = ");
  Serial.println(dev2_period_time);
  */
  if ((timepassed > dev2_period) || (timepassed < 0))
  {
	Serial.println("doing the long one!!!");
	Serial.print("    timepassed = ");
	Serial.print(timepassed);
	Serial.print("    dev2_period(60sec) = ");
	Serial.print(dev2_period);
	Serial.print("    millis = ");
	Serial.print(millis());
	Serial.print("    dev2_period_time = ");
	Serial.println(dev2_period_time);
	lightOutput = analogRead(lightPin);
	
	//send data
	theData.deviceID = 2;
	theData.var1_usl = millis();
	theData.var2_float = lightOutput;
	theData.var3_float = 0;
	radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData));
	
	//reset timer
	dev2_period_time = millis();
	dev2_change_time = millis();  //count this sensor examination 
  }
  
  //examine for change every change_time seconds;
  timepassed = millis() - dev2_change_time;
  if ((timepassed > dev2_change) || (timepassed < 0))
  {
    Serial.println("doing the shooooor!");
    lightOutput = analogRead(lightPin);
	
	if ((lightOutput > (dev2_current_value + dev2_change_amt)) || (lightOutput < (dev2_current_value - dev2_change_amt)))
	{
        Serial.println("change detected");
		dev2_current_value = lightOutput;
		//send data
		theData.deviceID = 2;
		theData.var1_usl = millis();
		theData.var2_float = lightOutput;
		theData.var3_float = 0;
		radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData));
	}
    dev2_change_time = millis();
  }

  delay(200); 
	
  //-------------------------------------------------------------------------------
  // deviceID = 3;  ultrasonic
  //-------------------------------------------------------------------------------
  unsigned long duration;
  float inches, cm;
  
  //send every x seconds
  timepassed = millis() - dev3_period_time;
  if ((timepassed > dev3_period) || (timepassed < 0))
  {
	Serial.println("doing the long one dev 3!!!");
	Serial.print("    timepassed = ");
	Serial.print(timepassed);
	Serial.print("    dev3_period(60sec) = ");
	Serial.print(dev3_period);
	Serial.print("    millis = ");
	Serial.print(millis());
	Serial.print("    dev3_period_time = ");
	Serial.println(dev3_period_time);
	
	
	  //get data
	  digitalWrite(trigPin, LOW);
	  delayMicroseconds(2);
	  digitalWrite(trigPin, HIGH);
	  delayMicroseconds(15);
	  digitalWrite(trigPin, LOW);
	  duration = pulseIn(pingPin, HIGH);
	  // convert the time into a distance
	  inches = microsecondsToInches(duration);
	  Serial.print(inches);
	  Serial.println(" inches");
	  //send data
	  theData.deviceID = 3;
	  theData.var1_usl = millis();
	  theData.var2_float = inches;
	  theData.var3_float = 0;
	  radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData));
	
	//reset timer
	dev3_period_time = millis();
	dev3_change_time = millis();  //count this sensor examination 
  }
  
  //examine for change every change_time seconds;
  timepassed = millis() - dev3_change_time;
  if ((timepassed > dev3_change) || (timepassed < 0))
  {
    Serial.println("doing the shooooor!");
    
	  //get data
	  digitalWrite(trigPin, LOW);
	  delayMicroseconds(2);
	  digitalWrite(trigPin, HIGH);
	  delayMicroseconds(15);
	  digitalWrite(trigPin, LOW);
	  duration = pulseIn(pingPin, HIGH);
	  // convert the time into a distance
	  inches = microsecondsToInches(duration);
	  Serial.print(inches);
	  Serial.println(" inches");
	  
	  
	if ((inches > (dev3_current_value + dev3_change_amt)) || (inches < (dev3_current_value - dev3_change_amt)))
	{
      Serial.println("change detected");
	  dev3_current_value = inches;
	  //send data
	  theData.deviceID = 3;
	  theData.var1_usl = millis();
	  theData.var2_float = inches;
	  theData.var3_float = 0;
	  radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData));
	}
    dev2_change_time = millis();
  }

  
  delay(200);

  //-------------------------------------------------------------------------------
  // deviceID = 4;  //temperature humidity
  //-------------------------------------------------------------------------------
  //DHT11 --------------------------------
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float temp_F;
  //send every x seconds
  timepassed = millis() - dev4_period_time;
  if ((timepassed > dev4_period) || (timepassed < 0))
  {
	  if (isnan(t) || isnan(h)) {
		Serial.println("Failed to read from DHT");
	  } else {
		Serial.print("Humidity: "); 
		Serial.print(h);
		Serial.print(" %\t");
		Serial.print("Temperature: "); 
		Serial.print(t);
		Serial.print(" *C\t");
		temp_F = t * 9/5+32;
		Serial.print(temp_F);
		Serial.println(" *F");
	  }
	  //send data
	  theData.deviceID = 4;
	  theData.var1_usl = millis();
	  theData.var2_float = temp_F;
	  theData.var3_float = h;
	  radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData));

          dev4_period_time = millis();
  }
  
}//end loop

//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
float microsecondsToInches(long microseconds)
{
  // According to Parallax's datasheet for the PING))), there are
  // 73.746 microseconds per inch (i.e. sound travels at 1130 feet per
  // second).  This gives the distance travelled by the ping, outbound
  // and return, so we divide by 2 to get the distance of the obstacle.
  // See: http://www.parallax.com/dl/docs/prod/acc/28015-PING-v1.3.pdf
  return microseconds / 74.0 / 2.0;
}
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
float microsecondsToCentimeters(long microseconds)
{
  // The speed of sound is 340 m/s or 29 microseconds per centimeter.
  // The ping travels out and back, so to find the distance of the
  // object we take half of the distance travelled.
  return microseconds / 29 / 2;
}
