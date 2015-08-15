/*
Author: Eric Tsai
License: CC-BY-SA, https://creativecommons.org/licenses/by-sa/2.0/
Date: 9-22-2014:  clean up comment for first post
File: Laundry_room.ino
This sketch is for a wired Arduino w/ RFM69 wireless transceiver
Monitors dryer status, uses PIR sensor to indicate empty load.
Notification for water leaks.
Light sensor.
to gateway. See OpenHAB configuration below.

1) Update encryption string "ENCRYPTKEY"
2)
*/

/*
OpenHAB configuration

Item Definition
Number itm_dryer "Dryer is [MAP(laundry.map):%s]" (ALL) {mqtt="<[mymosquitto:3052:state:default]"}

Sitemap Definition
Text item=itm_dryer valuecolor=[<1="black",==1="green",>1="orange"] labelcolor=[2="orange", 1="green", 0="black"]


Create a file <laundry.map> in /configuration/transform
0=Off & Empty
1=Running
2=Done

Reference
https://github.com/openhab/openhab/wiki/Explanation-of-Items



*/

/* MQTT topic addressing
node = 30

device ID:
2 = 3022 = Temperature_F
  = 3023 = Humidity
3 = 3021 = Water Present
4 = 3042 = Washer status (not implemented)
5 = 3052 = Dryer status (implemented)
6 = 3062 = Light

Pins
A2 = light sensor
3 = water leak
5 = PIR sensor
6 = sound 1
7 = DHT11 sensor
8 = sound 2


*/

//RFM69 --------------------------------------------------------------------------------------------------
#include <RFM69.h>
#include <SPI.h>
#define NODEID 30 //unique for each node on same network
#define NETWORKID 101 //the same on all nodes that talk to each other
#define GATEWAYID 1
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
//#define FREQUENCY RF69_433MHZ
//#define FREQUENCY RF69_868MHZ
#define FREQUENCY RF69_915MHZ
#define ENCRYPTKEY "xxxxxxxxxxxxxxxx" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ACK_TIME 30 // max # of ms to wait for an ack
#define LED 9 // Moteinos have LEDs on D9
#define SERIAL_BAUD 9600 //must be 9600 for GPS, use whatever if no GPS
boolean debug = 0;
//struct for wireless data transmission
typedef struct {	
  int nodeID; //node ID (1xx, 2xx, 3xx); 1xx = basement, 2xx = main floor, 3xx = outside
  int deviceID;	//sensor ID (2, 3, 4, 5)
  unsigned long var1_usl; //uptime in ms
  float var2_float; //sensor data?
  float var3_float;	//battery condition?
} Payload;
Payload theData;
char buff[20];
byte sendSize=0;
boolean requestACK = false;
RFM69 radio;
//end RFM69 ------------------------------------------

/*
// gas sensor================================================
int GasSmokeAnalogPin = 0; // potentiometer wiper (middle terminal) connected to analog pin
int gas_sensor = -500; // gas sensor value, current
int gas_sensor_previous = -500; //sensor value previously sent via RFM
*/

//temperature / humidity =====================================
#include "DHT.h"
#define DHTPIN 7 // digital pin we're connected to
#define DHTTYPE DHT11 // DHT 22 (AM2302) blue one
//#define DHTTYPE DHT21 // DHT 21 (AM2301) white one
// Initialize DHT sensor for normal 16mhz Arduino
DHT dht(DHTPIN, DHTTYPE);



// Light sensor ===============================================
int lightAnalogInput = A2; //analog input for photo resistor
int lightValue = -50;
int lightValue_previous = -50;

// PIR sensor ================================================
// used to clear a completed load (to know that it's been emptied)
int PirInput = 5;
int PIR_status = 0;
int PIR_reading = 0;
int PIR_reading_previous = 0;

// sound sensor 1 (Dryer) ==============================================
int soundInput1 = 6; //sound sensor digital input pin
int sound_status1 = 0;
int sound_reading1 = 0; //reading =1 mean no noise, 0=noise
int sound_reading1_previous = 0;
int sound_1_device_state = 0;  //1 = running, 0 = empty, 2 = complete

unsigned long sound_time_1 = 0;   //millis of last reading
int sound_count_1 = 0;   //number of captures
int sound_detected_count_1 = 0; //number of readings showing sound


// water leak sensor ==========================================
unsigned long water_time;
int waterInput = 3;   //pin for water detection
int water_reading = 0;  //for reading water pin


// timings
unsigned long pir_time;
unsigned long sound_time;
unsigned long temperature_time;
unsigned long light_time;
unsigned long light_time_send;


void setup()
{
  Serial.begin(9600); // setup serial
  //RFM69-------------------------------------------
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  #ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
  #endif
  radio.encrypt(ENCRYPTKEY);
  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  theData.nodeID = NODEID; //this node id should be the same for all devices in this node
  //end RFM--------------------------------------------
  
  
  dht.begin();  //temperature & humidity sensor
  
  pinMode(soundInput1, INPUT);  //sound sensor 1 (dryer)
  
  pinMode(waterInput, INPUT);    //water detection
  
  //initialize times
  sound_time = millis();
  temperature_time = millis();
  pinMode(PirInput, INPUT);  //PIR sensor
}
void loop()
{
  unsigned long time_passed = 0;

  
 

  //===============================================================
  //Water Leak sensor
  //Device #3
  //===============================================================
  water_reading = digitalRead(waterInput);    //0 = no water, 1 = water (backwards)
  if (water_reading)
  Serial.println("no water");
  else
  {
  Serial.println("water det ");
  delay(1000);
  }
  
  if ((water_reading == 0) && ( ((millis() - water_time)>7000)||( (millis() - water_time)< 0)) ) //meaning there was sound
  {
    water_time = millis(); //update gas_time_send with when sensor value last transmitted

    theData.deviceID = 3;
    theData.var1_usl = millis();
    theData.var2_float = 1;
    theData.var3_float = 101;	//null value;
    radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData));
    Serial.print("water detected!!! ");

  }
 
  
  //===================================================================
  //device PIR
  //note:  this PIR sensor goes straight to I/O, no pull down resistor.
  // 1 = detected movement
  PIR_reading = digitalRead(PirInput);
  //if (PIR_reading == 1)
  //Serial.println("PIR = 1");
  //else
  //Serial.println("PIR = 0");

  // if someone is in the laundry room and washer/dryer is complete (not emptied)
  if ((PIR_reading == 1) && (sound_1_device_state == 2))
  {
    sound_1_device_state = 0;
    theData.deviceID = 5;
    theData.var1_usl = millis();
    theData.var2_float = 0;
    theData.var3_float = 11223;	//null value;
    radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData));
  }

  
  //===================================================================
  //device #5
  //Dryer Running Sound Sensor

  //soundValue = analogRead(soundAnalogInput);
  //Serial.print("sound analog = ");
  //Serial.print(soundValue);
  
  
  //deal with millis rollover
  if (sound_time_1 > millis())
  {
    sound_time_1 = millis();
  }
  
  //capture sound as fast as we can.  If there is sound, mark it as so.
  int sound = digitalRead(soundInput1);
  if (sound == 0)
  {
    sound_reading1 = 0;
  }
  
  //count sound as for real only every 1/2 second.
  if ((millis() - sound_time_1)>500)
  {
    sound_time_1 = millis();    //reset sound_time_1 to wait for next Xms
  
    //sound_reading1 = digitalRead(soundInput1);  // 1 = no noise, 0 = noise!!
    
    /*
    Serial.print(sound_count_1);
    Serial.print(" out of ");
    Serial.print(sound_detected_count_1);
    Serial.print(" state= ");
    Serial.print(sound_1_device_state);
    if (!sound_reading1)
    Serial.println("   sound detected");
    else
    Serial.println("   no sound detected");
    */
    
    sound_count_1 = sound_count_1 + 1;          //count how many times we listened
    if (sound_reading1 == 0)                    //count how many times detected sound
    {
      sound_detected_count_1 = sound_detected_count_1 + 1;
    }
    
    sound_reading1 = 1;        //reset back to no sound (1 = no sound)
  }//end reading every second
  
  //after X number of sound checks...
  if (sound_count_1 >= 30)
  {
    //sound_count_1 = number of times sensor listened
    //sound_detected_count_1 = number of times heard sound
    //sound_1_device_state:  0=emptied, 1=running, 2=cycle complete (but not emptied)
    
    // sound_1_device_state = 1 = running 
    Serial.print("checking checking evaluating sound");
    if ((sound_detected_count_1 >= 5) && ((sound_1_device_state == 0) || (sound_1_device_state == 2)))  //number of times sensor registered sound
    {
      //Serial.println("running!");
      sound_1_device_state = 1;
    
      theData.deviceID = 4;
      theData.var1_usl = millis();
      theData.var2_float = 1;          //1 = device running
      theData.var3_float = 11223;	    //doesn't mean a thing
      radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData));
    }
    
    // sound_1_device_state = 2 = Cycle complete (but not emptied) 
    else if ((sound_detected_count_1 < 3) && (sound_1_device_state == 1))
    {
      //Serial.println("not running!!");
      sound_1_device_state = 2;
      
      theData.deviceID = 5;
      theData.var1_usl = millis();
      theData.var2_float = 2;
      theData.var3_float = 11223;	//null value;
      radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData));
      
      //reset how often sensor has listened and how many sound events detected
    }
    sound_count_1 = 0;
    sound_detected_count_1 = 0;
  } //end if count_count_1
  

  //===================================================================
  //device #2
  //temperature / humidity
  time_passed = millis() - temperature_time;
  if (time_passed < 0)
  {
  temperature_time = millis();
  }

  //only report temps/humidity after X seconds
  if (time_passed > 360000)
  {
    float h = dht.readHumidity();
    // Read temperature as Celsius
    float t = dht.readTemperature();
    // Read temperature as Fahrenheit
    float f = dht.readTemperature(true);
    // Check if any reads failed and exit early (to try again).
    if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
    }
    Serial.print("Humidity=");
    Serial.print(h);
    Serial.print(" Temp=");
    Serial.println(f);
    temperature_time = millis();
    //send data
    theData.deviceID = 2;
    theData.var1_usl = millis();
    theData.var2_float = f;
    theData.var3_float = h;
    radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData));
    delay(1000);
  }

  
  //===================================================================
  //device #6
  //light
  time_passed = millis() - light_time;
  if (time_passed < 0)
  {
    light_time = millis();
    light_time_send = -70000;
  }
  if (time_passed > 2000) //how often to examine the sensor analog value
  {
    light_time = millis();	//update time when sensor value last read
    lightValue = 0;
    //analog value: Less than 100 is dark. greater than 500 is room lighting
    lightValue = analogRead(lightAnalogInput);
    if ((lightValue < (lightValue_previous - 50)) || ((lightValue > (lightValue_previous + 100)) || (705000 < (millis() - light_time_send))) )
    {
      light_time_send = millis();
      theData.deviceID = 6;
      theData.var1_usl = millis();
      theData.var2_float = lightValue;
      theData.var3_float = lightValue + 20;
      radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData));
      lightValue_previous = lightValue;
      Serial.print("light RFM =");
      Serial.println(lightValue);
    }	
    //start debug code
    if (0)
    {
      Serial.print("light analog = ");
      Serial.println(lightValue);
    }

  }// end if millis time_passed >
}//end loop

