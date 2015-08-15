/*
Author:  Eric Tsai
License:  CC-BY-SA, https://creativecommons.org/licenses/by-sa/2.0/
Date:  9-18-2014
File: dog_GPS_send.ino
Dog Tracker sketch.  Sends GPS coordinate to gateway.

Modifications Needed:
1)  Update encryption string "ENCRYPTKEY"
2)  
*/


/*
NOTES:
1)  RFM69 pin:
    MOSI = 11
    MISO = 12
    SCK = 13
    SS = 10
    
    D_pin 5 = poop incline
    D_pin 6 = left side
    D_pin 7 = right side
*/

int pin_poop = 5;  //poop incline (1 = poop)
int pin_left = 6;  //left (1 = upright)
int pin_right = 7; //right (1 = upright)


//for RFM69
#include <RFM69.h>
#include <SPI.h>


//for GPS:
#include <SoftwareSerial.h>
#include <TinyGPS.h>


//#include <SPIFlash.h>


//RFM setup stuff
#define NODEID        42    //unique for each node on same network
#define NETWORKID     101  //the same on all nodes that talk to each other
#define GATEWAYID     1
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
//#define FREQUENCY   RF69_433MHZ
//#define FREQUENCY   RF69_868MHZ
#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "xxxxxxxxxxxxxxxx" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ACK_TIME      30 // max # of ms to wait for an ack
#define LED           9  // Moteinos have LEDs on D9
#define SERIAL_BAUD   9600  //must be 9600 for GPS, use whatever if no GPS

//GPS setup stuff
// RX, TX;  Pin 3 (RX) of Arduino goes to RX of GPS
SoftwareSerial mySerial(4, 3); 
TinyGPS gps;
void gpsdump(TinyGPS &gps);
void printFloat(double f, int digits = 2);
int printalldata = 0;

//RFM transmission structure
typedef struct {		
  int           nodeID; 		//node ID (1xx, 2xx, 3xx);  1xx = basement, 2xx = main floor, 3xx = outside
  int			deviceID;		//sensor ID (2, 3, 4, 5)
  unsigned long var1_usl; 		//uptime in ms
  float         var2_float;   	//sensor data?
  float		var3_float;		//battery condition?
} Payload;
Payload theData;

//MQTT topic names
//4221 = temperature
//4222 = normal position, lat
//4223 = normal position, long
//4231 = poop position, deg
//4232 = poop position, lat
//4233 = poop position, long


int TRANSMITPERIOD = 2000; //transmit a packet to gateway so often (in ms)
char buff[20];
//byte sendSize=0;
boolean requestACK = false;




//timers
unsigned long last_gps = 0;
unsigned long last_poop = 0;

int poop_deg = 0;
int temperature = 0;
int position_flag = 2; //1 = poop, 2=walking, 3=left side laying down 4=right side laying down
int position_hold = 2;
int position_current = 2;
unsigned long hold_time = 0;

unsigned long temp_time = 0; //millis time of last temperature send

RFM69 radio;

void setup() 
{
  pinMode(pin_poop, INPUT);
  pinMode(pin_left, INPUT);
  pinMode(pin_right, INPUT);

  //RFM69 --------------------------------
  Serial.begin(SERIAL_BAUD);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  #ifdef IS_RFM69HW
    radio.setHighPower(); //uncomment only for RFM69HW!
  #endif
  radio.encrypt(ENCRYPTKEY);
  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  //--------------------------------------
  
  //GPS ----------------------------------
  mySerial.begin(9600);
  delay(1000);
  Serial.println("uBlox Neo 6M");
  Serial.print("Testing TinyGPS library v. "); Serial.println(TinyGPS::library_version());
  Serial.println("by Mikal Hart");
  Serial.println();
  Serial.print("Sizeof(gpsobject) = "); Serial.println(sizeof(TinyGPS));
  Serial.println();
  
  
}//end setup



//delete?
long lastPeriod = -1;
int mycounter = 1;    //not really used for much, just counting radio sends

void loop() {
  //process any serial input
  //check for any received packets.  Not need for Dog transmitter, but kept for future
  if (radio.receiveDone())
  {
    Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
    for (byte i = 0; i < radio.DATALEN; i++)
      Serial.print((char)radio.DATA[i]);
    Serial.print("   [RX_RSSI:");Serial.print(radio.RSSI);Serial.print("]");

    if (radio.ACK_REQUESTED)
    {
      radio.sendACK();
      Serial.print(" - ACK sent");
      delay(10);
    }
    //Blink(LED,5);
    Serial.println();
  }
  
  //read digital inputs for dog position
  bool pos_poop, pos_left, pos_right; //digital input results
  pos_poop = digitalRead(pin_poop);
  pos_left = digitalRead(pin_left);
  pos_right = digitalRead(pin_right);
  
  
  //determine instantaneous dog disposition
  if ((pos_poop == 0) && (pos_left == 1) && (pos_right == 1))  //upright
  {
    position_current = 2;
  }
  else if ((pos_left == 0) && (pos_right == 1))  //left
  {
    position_current = 3;
  } 
  else if ((pos_left == 1) && (pos_right == 0))  //right
  {
    position_current = 4;
  }
  else if ((pos_poop == 1) && (pos_left == 1) && (pos_right == 1))  //poop
  {
    position_current = 1;
  }
  
  //debounce position
  if (position_current == position_hold)
  {
    //do nothing
  }
  if (position_current !=  position_hold)
  {
    hold_time = millis();
    position_hold = position_current;
  }
  if ((millis() - hold_time) > 5500) //if position held for more than x ms
  {
    position_flag = position_hold;
  }
  
  
  //integer divisions are floor functions
  //millis() roll over assumed not an issues (50 days)
  int currPeriod = millis()/TRANSMITPERIOD;

  //sends GPS data asynchronously with GPS coordinate updates
  //sends GPS data every TRANSITPERIOD
  //if (currPeriod != lastPeriod)
  Serial.print("   last GPS");
  Serial.print(last_gps);
  Serial.print("   millis");
  Serial.println(millis());
  
  
  if ((millis() - last_gps) > 2500)
  {
    //lastPeriod=currPeriod;
    last_gps = millis();
    
    //fill struct w/ values.  Assume var2 and var3 already filled
    Serial.println("===== tag sending data");
    mycounter = mycounter + 1;
    theData.nodeID = NODEID;  //identifies the sending node
    if (position_flag == 1) //1 = poop
    {
      Serial.println("Pooping");
      theData.deviceID = 3;     //3=poop
      theData.var1_usl = poop_deg;
    }
    else if ((position_flag > 1) || (position_flag==0))
    {
      Serial.println("Regular");
      theData.deviceID = 2;     //2 = not poop
      theData.var1_usl = position_flag; //2=walking, 3=left side laying down 4=right side laying down
    }
    
    //for now, send mycounter.  Later, send temperature
    //theData.var1_usl = mycounter; //counter...for curiosity sake
    //byte temperature =  radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
    //byte fTemp = 1.8 * temperature + 32; // 9/5=1.8
    //theData.var1 = fTemp;
    
    //send data!
    //theData.var2_float = 44.702388;
    //theData.var3_float = -93.230855;
    radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData));
    Serial.println("Data sent!");
    
    if ((millis() - temp_time)>70000)
    {
      temp_time = millis();
      
      delay(2000);
      byte temperature = radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
      int fTemp = 1.8 * temperature + 32; // 9/5=1.8
      theData.deviceID = 4;   //4241 is temperature
      theData.var1_usl = fTemp;
      radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData));
    }
  }
  

  //GPS
  bool newdata = false;
  unsigned long start = millis();

  // Every x seconds look for new data from GPS module
  while ((millis() - start) < 1500) 
  {
    if (mySerial.available()) 
    {
      //Serial.println("avialable new serial data");
      char c = mySerial.read();
      //Serial.print(c);  // uncomment to see raw GPS data
      if (gps.encode(c)) 
      {
        newdata = true;
         break;  // uncomment to print new data immediately!
      }//end if gps
      newdata = true; //kind of forgot why I have two of these...seems to work ok.
    }//end if
  }//end while
  
  if (newdata) {
    if (printalldata == 1)
    {
    Serial.println("Acquired Data");
    Serial.println("-------------");
    }
    gpsdump(gps);
    if (printalldata == 1)
    {
    Serial.println("-------------");
    Serial.println();
    }
  }//end if newdata
}//end loop

//not important
void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}


//Split up GPS data into individual variables.
//This function is where we're taking latitude and longtitude and
//stuffing it into the wireless transmission data structure
//theData.var2_float,  theData.var3_float
void gpsdump(TinyGPS &gps)
{
  long lat, lon;
  float flat, flon;
  unsigned long age, date, time, chars;
  int year;
  byte month, day, hour, minute, second, hundredths;
  unsigned short sentences, failed;

  gps.get_position(&lat, &lon, &age);
  //Serial.print("Lat/Long(10^-5 deg): "); Serial.print(lat); Serial.print(", "); Serial.print(lon); 
  //Serial.print(" Fix age: "); Serial.print(age); Serial.println("ms.");
  
  // On Arduino, GPS characters may be lost during lengthy Serial.print()
  // On Teensy, Serial prints to USB, which has large output buffering and
  //   runs very fast, so it's not necessary to worry about missing 4800
  //   baud GPS characters.

  gps.f_get_position(&flat, &flon, &age);
  if (printalldata == 1)
    Serial.print("Lat/Long(float): "); 
  theData.var2_float = flat;
  theData.var3_float = flon;
  //theData.var2_float = 3;
  //theData.var3_float = -92.12345;
  printFloat(flat, 5); Serial.print(", "); printFloat(flon, 5);
  if (printalldata == 1)
  {
    Serial.print(" Fix age: "); Serial.print(age); Serial.println("ms.");
  }
  gps.get_datetime(&date, &time, &age);
  //Serial.print("Date(ddmmyy): "); Serial.print(date); Serial.print(" Time(hhmmsscc): ");
  //  Serial.print(time);
  //Serial.print(" Fix age: "); Serial.print(age); Serial.println("ms.");

  gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);
  if (printalldata == 1)
  {
    Serial.print("Date: "); Serial.print(static_cast<int>(month)); Serial.print("/"); 
      Serial.print(static_cast<int>(day)); Serial.print("/"); Serial.print(year);
    Serial.print("  Time: "); Serial.print(static_cast<int>(hour)); Serial.print(":"); 
      Serial.print(static_cast<int>(minute)); Serial.print(":"); Serial.print(static_cast<int>(second));
      Serial.print("."); Serial.print(static_cast<int>(hundredths));
    Serial.print("  Fix age: ");  Serial.print(age); Serial.println("ms.");
  }
  //Serial.print("Alt(cm): "); Serial.print(gps.altitude()); Serial.print(" Course(10^-2 deg): ");
    //Serial.print(gps.course()); Serial.print(" Speed(10^-2 knots): "); Serial.println(gps.speed());
    
  if (printalldata == 1)
  {
  Serial.print("Alt(float): "); printFloat(gps.f_altitude()); Serial.print(" Course(float): ");
    printFloat(gps.f_course()); Serial.println();
  //Serial.print("Speed(knots): "); printFloat(gps.f_speed_knots()); 
  Serial.print(" (mph): ");
    printFloat(gps.f_speed_mph());
  //Serial.print(" (mps): "); printFloat(gps.f_speed_mps()); Serial.print(" (kmph): ");
  //  printFloat(gps.f_speed_kmph()); Serial.println();

  gps.stats(&chars, &sentences, &failed);
  Serial.print(" Stats: characters: "); Serial.print(chars); Serial.print(" sentences: ");
    Serial.print(sentences); Serial.print(" failed checksum: "); Serial.println(failed);
  }
} //end gpsdump


//only used for serial monitor print outs, not impact on GPS transmission
void printFloat(double number, int digits)
{
  // Handle negative numbers
  if (number < 0.0) {
     Serial.print('-');
     number = -number;
  }

  // Round correctly so that print(1.999, 2) prints as "2.00"
  double rounding = 0.5;
  for (uint8_t i=0; i<digits; ++i)
    rounding /= 10.0;
  
  number += rounding;

  // Extract the integer part of the number and print it
  unsigned long int_part = (unsigned long)number;
  double remainder = number - (double)int_part;
  Serial.print(int_part);

  // Print the decimal point, but only if there are digits beyond
  if (digits > 0)
    Serial.print("."); 

  // Extract digits from the remainder one at a time
  while (digits-- > 0) {
    remainder *= 10.0;
    int toPrint = int(remainder);
    Serial.print(toPrint);
    remainder -= toPrint;
  }
} //end printFloat
