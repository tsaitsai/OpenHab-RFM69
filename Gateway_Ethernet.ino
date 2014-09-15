/*
Author:  Eric Tsai
License:  CC-BY-SA, https://creativecommons.org/licenses/by-sa/2.0/
Date:  7-21-2014
File: Ethernet_Gateway.ino
This sketch takes data struct from I2C and publishes it to
mosquitto broker.

Modifications Needed:
1)  Update mac address "mac[]"
2)  Update MQTT broker IP address "server[]"
*/

#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <PubSubClient.h>

//I2C receive device address
const byte MY_ADDRESS = 42;    //I2C comms w/ other Arduino

//Ethernet
byte mac[]    = {  0x90, 0xA2, 0xDA, 0x0D, 0x11, 0x11 };
byte server[] = { 192, 168, 1, 51 };

IPAddress ip(192,168,2,61);
EthernetClient ethClient;
PubSubClient client(server, 1883, callback, ethClient);
unsigned long keepalivetime=0;
unsigned long MQTT_reconnect=0;


//use LED for indicating MQTT connection status.
int led = 13;
bool conn_ok;

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
}


void setup() 
{
  //ethernet
  //Ethernet.begin(mac, ip);
  Wire.begin (MY_ADDRESS);
  Serial.begin (9600);
  
  Serial.println("starting");
  
  pinMode(led, OUTPUT);
  
  //wait for IP address
  
  while (Ethernet.begin(mac) != 1)
 
  {
    Serial.println("Error getting IP address via DHCP, trying again...");
    delay(3000);
  }
  
  //Ethernet.begin(mac, ip);
  Serial.println("ethernet OK");
  keepalivetime=millis();
  Wire.onReceive (receiveEvent);

  while (client.connect("arduinoClient") != 1)
  {
    Serial.println("Error connecting to MQTT");
    delay(3000);
  }
  MQTT_reconnect = millis();
  Serial.println("setup complete");
  
}  // end of setup



volatile struct 
   {
  int                   nodeID;
  int			sensorID;		
  unsigned long         var1_usl;
  float                 var2_float;
  float			var3_float;		//
  int                   var4_int;
   } SensorNode;

int sendMQTT = 0;
volatile boolean haveData = false;

void loop() 
{
  
  if (haveData)
  {
    Serial.print ("Received Device ID = ");
    Serial.println (SensorNode.sensorID);  
    Serial.print ("    Time = ");
    Serial.println (SensorNode.var1_usl);
    Serial.print ("    var2_float ");
    Serial.println (SensorNode.var2_float);
    
    sendMQTT = 1;
    
    /*
    if (client.connect("arduinoClient"))
    {
      int varnum = 1;
      char buff_topic[6];
      char buff_message[6];
      sprintf(buff_topic, "%02d%01d%01d", SensorNode.nodeID, SensorNode.sensorID, varnum);
      Serial.println(buff_topic);
      dtostrf (SensorNode.var2_float, 4, 1, buff_message);
      client.publish(buff_topic, buff_message);
    }
    */
    
    haveData = false;
  }  // end if haveData
  
  
  if (sendMQTT == 1)
  {

      Serial.println("starting MQTT send");
      
      conn_ok = client.connected();
      if (conn_ok==1)
      {
        digitalWrite(led, HIGH);
        Serial.println("MQTT connected OK from MQTT Send");
      }
      else
      {
        digitalWrite(led, LOW);
        Serial.println("MQTT NOT connected OK from MQTT Send");
      }
      
      //no connection, reconnect
      if (conn_ok == 0)
      {
        client.disconnect();
        delay(5000);
        while (client.connect("arduinoClient") != 1)
        {
          digitalWrite(led, LOW);
          Serial.println("Error connecting to MQTT");
          delay(4000);
          digitalWrite(led, HIGH);
        }
        digitalWrite(led, HIGH);
        Serial.println("reconnected to MQTT");
        MQTT_reconnect = millis();
      }


      int varnum;
      char buff_topic[6];
      char buff_message[12];      

      
      /*
      //send var1_usl
      varnum = 2;
      buff_topic[6];
      buff_message[12];
      sprintf(buff_topic, "%02d%01d%01d", SensorNode.nodeID, SensorNode.sensorID, varnum);
      Serial.println(buff_topic);
      dtostrf (SensorNode.var1_usl, 10, 1, buff_message);
      client.publish(buff_topic, buff_message);
      */
      
      //send var2_float
      varnum = 2;
      buff_topic[6];
      buff_message[7];
      sprintf(buff_topic, "%02d%01d%01d", SensorNode.nodeID, SensorNode.sensorID, varnum);
      Serial.println(buff_topic);
      dtostrf (SensorNode.var2_float, 2, 1, buff_message);
      client.publish(buff_topic, buff_message);
      
      delay(200);
      
      //send var3_float
      varnum = 3;
      sprintf(buff_topic, "%02d%01d%01d", SensorNode.nodeID, SensorNode.sensorID, varnum);
      Serial.println(buff_topic);
      dtostrf (SensorNode.var3_float, 2, 1, buff_message);
      client.publish(buff_topic, buff_message);

      delay(200);
      
      //send var4_int, RSSI
      varnum = 4;
      sprintf(buff_topic, "%02d%01d%01d", SensorNode.nodeID, SensorNode.sensorID, varnum);
      Serial.println(buff_topic);
      sprintf(buff_message, "%04d%", SensorNode.var4_int);
      client.publish(buff_topic, buff_message);
      
      sendMQTT = 0;
      Serial.println("finished MQTT send");
  }//end if sendMQTT
  
  
  client.loop();
  
  
  if ((millis() - MQTT_reconnect) > 60000)
  {
    conn_ok = client.connected();
    if (conn_ok==1)
    {
      digitalWrite(led, HIGH);
      Serial.println("MQTT connected OK");
    }
    else
    {
      digitalWrite(led, LOW);
      Serial.println("MQTT NOT connected OK");
    }
    
    //no connection, reconnect
    if (conn_ok == 0)
    {
      client.disconnect();
      delay(5000);
      while (client.connect("arduinoClient") != 1)
      {
        digitalWrite(led, LOW);
        Serial.println("Error connecting to MQTT");
        delay(4000);
        digitalWrite(led, HIGH);
      }
      digitalWrite(led, HIGH);
    }

    Serial.println("reconnected to MQTT");
    MQTT_reconnect = millis();
  }

}  // end of loop




// called by interrupt service routine when incoming data arrives
void receiveEvent (int howMany)
{
  if (howMany < sizeof SensorNode)
    return;
    
  // read into structure
  byte * p = (byte *) &SensorNode;
  for (byte i = 0; i < sizeof SensorNode; i++)
    *p++ = Wire.read ();
    
  haveData = true;     
}
