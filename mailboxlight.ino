// This #include statement was automatically added by the Particle IDE.
#include "OneWire/OneWire.h"

// This #include statement was automatically added by the Spark IDE.
#include "core-library-DHT-sensor.h"

// This #include statement was automatically added by the Spark IDE.
#include "Sunrise.h"


#define DHTPIN D2    // Digital pin D2

// IMPORTANT !! Make sure you set this to your 
// sensor type.  Options: [DHT11, DHT22, DHT21, AM2301]
#define DHTTYPE DHT11  

DHT dht(DHTPIN, DHTTYPE);

float h;      // humidity
float t;      // temperature
char h1[10];  // humidity string
char t1[10];  // temperature string
int f = 0;    // failed?

char resultstr[128]; // for logging 64

// for publishing
unsigned long lastTime = 0UL; 
unsigned long interval = 300000UL;  

int init = 0;

unsigned long lastUpdate = 0UL; 

int LED = D7;
int State = 0;

int RELAY1 = D1;
int relayState = 1;

char sr[10];  // sunrise string
char ss[10];  // sunset string

#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)
unsigned long lastSync = millis();

    const float Latitude = 35.69, Longitude = 139.69;  // in decimals, used for Sunrise and Sunset
    const long Timezone = +9;  // UTC/GMT offset (JST = UTC+9)
    // const int dstOffset = 0;  // DST/Summer Time offset in seconds (0)

    // create a Sunrise object
    Sunrise mySunrise(Latitude,Longitude,Timezone);
    int sunrise,sunset;

int sensorPin = A4;            // select the input pin for the ldr
unsigned int sensorValue = 0;  // variable to store the value coming from the ldr


//DS18S20 Signal pin on digital 2
int DS18S20_Pin = D3; 
//Temperature chip i/o
OneWire ds(DS18S20_Pin);

void setup() {

    // Scale the RGB LED brightness to 25%
    RGB.control(true);
    RGB.brightness(64);
    RGB.control(false);

    mySunrise.Actual(); //Actual, Civil, Nautical, Astronomical

    // Set time zone to Japan Standard Time (JST)
    Time.zone(Timezone); 

    // Serial.print(Time.now()); // 1400647897
    // Serial.print(Time.timeStr()); // Wed May 21 01:08:47 2014 
    
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);
    Spark.function("led", switchLED);
    Spark.variable("ledState", &State, INT);
    
    pinMode(RELAY1, OUTPUT);
    digitalWrite(RELAY1, HIGH);
    Spark.function("switch", switchRELAY);
    Spark.variable("relayState", &relayState, INT);    
    
    Spark.variable("sunrise", &sr, STRING);    
    Spark.variable("sunset", &ss, STRING); 
    
    Spark.variable("humidity", &h1, STRING);
    Spark.variable("temperature", &t1, STRING);
    Spark.variable("status", &f, INT);
    dht.begin();

    Spark.variable("data", &resultstr, STRING);  

    // initialize sunset variables
    Spark.syncTime();
    update();
    
} 

void loop() {
    
    if ((millis() - lastSync) > 300000 && !init)  {
        // delayey one time initialization
        init = 1;
        Spark.syncTime();
        update();
    }       
    
    
    if (millis() - lastSync > ONE_DAY_MILLIS) {
        // Request time synchronization from the Spark Cloud
        Spark.syncTime();
        lastSync = millis();
        
        // calculate sunrise/sunset once a day 
        update();
    }   
    
    sensorValue = analogRead(sensorPin);
     if(sensorValue<400) { digitalWrite(LED, LOW); // set the LED off
     State = 0;
     } else { digitalWrite(LED, HIGH); State = 1; }  // set the LED on   
    
    
    // check if we need to switch on the light
    int current = (Time.hour() * 60) + Time.minute(); 
    if ((current > sunrise) && (current < sunset)) {
        // OFF
        digitalWrite(RELAY1, HIGH); 
        relayState = 1;         
    } else {
        // ON
        digitalWrite(RELAY1, LOW); 
        relayState = 0;    
    }
    
    f = 0;
    h = dht.readHumidity();
    t = dht.readTemperature();
    
    // t =  getTemp();

    if (t==NAN || h==NAN) {
        f = 1; // not a number, fail.
    }
    else {
        f = 0; // both numbers! not failed.
        sprintf(h1, "%.2f", h); // convert Float to String
        sprintf(t1, "%.2f", t);
        // format your data as JSON, don't forget to escape the double quotes
        // sprintf(resultstr, "{\"data1\":%f,\"data2\":%f}", h, t);         
    }

   // format your data as JSON, don't forget to escape the double quotes
    // sprintf(resultstr, "{\"current\":%i,\"sunrise\":%i,\"sunset\":%i}", current, sunrise, sunset);
    sprintf(resultstr, "{\"current\":\"%02i:%02i\",\"sunrise\":\"%s\",\"sunset\":\"%s\",\"temp\":\"%.2f\",\"rh\":\"%.2f\"}", Time.hour(), Time.minute(), sr, ss, t, h); 
 
    unsigned long now = millis();
    // publish data every n seconds
    if (now-lastTime>interval) {
        lastTime = now;
        // now is in milliseconds
        Spark.publish("Temp",t1);
        Spark.publish("RH",h1); 
    }

}

int switchLED(String args) {
    digitalWrite(LED, State == 1 ? LOW : HIGH);
    return State = State == 1 ? 0 : 1;
}

int switchRELAY(String args) {
    digitalWrite(RELAY1, relayState == 1 ? LOW : HIGH);
    return relayState = relayState == 1 ? 0 : 1;
}

void update() {

    // calculate today's sunrise/sunset
    // minutes past midnight of sunrise (6 am would be 360)
    sunrise=mySunrise.Rise(Time.month(),Time.day()); // (month,day) - january=1
    sprintf(sr, "%02i:%02i", mySunrise.Hour(), mySunrise.Minute());
    sunset=mySunrise.Set(Time.month(),Time.day()); 
    sprintf(ss, "%02i:%02i", mySunrise.Hour(), mySunrise.Minute()); 
    
    lastUpdate = millis();
}


float getTemp(){
 //returns the temperature from one DS18S20 in DEG Celsius

 byte data[12];
 byte addr[8];

 if ( !ds.search(addr)) {
   //no more sensors on chain, reset search
   ds.reset_search();
   return -1000;
 }

 if ( OneWire::crc8( addr, 7) != addr[7]) {
   Serial.println("CRC is not valid!");
   return -10001;
 }

 if ( addr[0] != 0x10 && addr[0] != 0x28) {
   Serial.print("Device is not recognized");
   return -1002;
 }

 ds.reset();
 ds.select(addr);
 ds.write(0x44,1); // start conversion, with parasite power on at the end

 byte present = ds.reset();
 ds.select(addr);  
 ds.write(0xBE); // Read Scratchpad

 
 for (int i = 0; i < 9; i++) { // we need 9 bytes
  data[i] = ds.read();
 }
 
 ds.reset_search();
 
 byte MSB = data[1];
 byte LSB = data[0];

 float tempRead = ((MSB << 8) | LSB); //using two's compliment
 float TemperatureSum = tempRead / 16;
 
 return TemperatureSum;
 
}


/* how to read:
curl https://api.spark.io/v1/devices/55555555/humidity?access_token=99999999

curl https://api.spark.io/v1/devices/55555555/temperature?access_token=99999999

curl https://api.spark.io/v1/devices/55555555/status?access_token=99999999

55555555 = Your Device ID
99999999 = Your Access Token ID

or put the URL's for variable requests right in your browser's address bar because they are GET requests:

https://api.spark.io/v1/devices/55555555/humidity?access_token=99999999`

https://api.spark.io/v1/devices/55555555/temperature?access_token=99999999

https://api.spark.io/v1/devices/55555555/status?access_token=99999999
*/