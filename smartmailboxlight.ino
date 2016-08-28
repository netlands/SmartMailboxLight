// This #include statement was automatically added by the Particle IDE.
#include "Adafruit_DHT/Adafruit_DHT.h"

// This #include statement was automatically added by the Particle IDE.
#include "Sunrise.h"

#include "QueueArray.h"

// for publishing
unsigned long lastTime = 0UL; 
unsigned long interval = 30000UL; //300000UL;  

int init = 0;

unsigned long lastUpdate = 0UL;

#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)
unsigned long lastSync = millis();

const float Latitude = 35.69, Longitude = 139.69;  // in decimals, used for Sunrise and Sunset
const long Timezone = +9;  // UTC/GMT offset (JST = UTC+9)
// const int dstOffset = 0;  // DST/Summer Time offset in seconds (0)
// create a Sunrise object
Sunrise mySunrise(Latitude,Longitude,Timezone);
int sunrise,sunset;

char sr[10];  // sunrise string
char ss[10];  // sunset string

int RELAYPIN = D1; //D1;
int relayState = 1;

int switchon(String command);
int switchoff(String command);

bool override(false);

#define DHTPIN D2     // what pin we're connected to
//#define DHTTYPE DHT11		// DHT 11 
#define DHTTYPE DHT22		// DHT 22 (AM2302)
//#define DHTTYPE DHT21		// DHT 21 (AM2301)

// Connect pin 1 (on the left) of the sensor to +5V
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

DHT dht(DHTPIN, DHTTYPE);

float h;      // humidity
float t;      // temperature
char h1[10];  // humidity string
char t1[10];  // temperature string

char resultstr[128]; // for logging 64


/*int photoresistor = A1; 
int prpower = A5; 
int analogvalue;
char p1[10];*/

char data[128];
char graphData[200];
// history
// create a queue of numbers.
QueueArray <int> queue;
QueueArray <int> graph;
unsigned long logDelay = 0UL;

void setup() {

    // Scale the RGB LED brightness to 25%
    RGB.control(true);
    RGB.brightness(64);
    RGB.control(false);

    mySunrise.Actual(); //Actual, Civil, Nautical, Astronomical

    // Set time zone to Japan Standard Time (JST)
    Time.zone(Timezone); 


    pinMode(RELAYPIN, OUTPUT);
    digitalWrite(RELAYPIN, HIGH);
    Particle.function("switchon", switchOn);
	Particle.function("switchoff", switchOff);
    Particle.function("status", Status);
    Particle.function("Switch", Switch);
    
    Particle.variable("sunrise", sr, STRING);    
    Particle.variable("sunset", ss, STRING); 
    
    Particle.variable("temperature", t1, STRING);    
    Particle.variable("humidity", h1, STRING);     

    /*Particle.variable("analogvalue", &analogvalue, INT);
    pinMode(photoresistor,INPUT); 
    pinMode(prpower,OUTPUT); */
    
    Particle.variable("data", resultstr, STRING); 

    // initialize sunset variables
    Particle.syncTime();
    update();

	Particle.function("getData", getGraphData);
	Particle.variable("history", graphData, STRING);
    Particle.variable("lastUpdate", &lastTime, INT);
    Particle.variable("interval", &interval, INT);

    pinMode(DHTPIN, INPUT_PULLUP);

	dht.begin();

}

void loop() {

    
    if ((millis() - lastSync) > 300000 && !init)  {
        // delay one time initialization
        init = 1;
        Particle.syncTime();
        update();
    }       
    
    
    if (millis() - lastSync > ONE_DAY_MILLIS) {
        // Request time synchronization from the Spark Cloud
        Particle.syncTime();
        lastSync = millis();
        
        // calculate sunrise/sunset once a day 
        update();

    }  
    
    unsigned long now = millis();
    // publish data every n seconds
    if (now-lastTime>interval) {
        lastTime = now; 
        // now is in milliseconds
        
         // check if we need to switch on the light
        int current = (Time.hour() * 60) + Time.minute(); 
        if ((current > sunrise) && (current < sunset)) {
            // OFF
            if (!override) {
                digitalWrite(RELAYPIN, HIGH); 
                relayState = 1; 
            }
        } else {
            // ON
            override = false;
            digitalWrite(RELAYPIN, LOW); 
            relayState = 0;    
        }        
        
        readDhtData();
        
        // log data
        if (graph.count() == 20){graph.pop();}
		graph.enqueue( round(t) ); // round(t*10)/10
        
    }    


		// log data every 10 minutes
		if (now-logDelay>600000UL) {
			logDelay = now;
			// keep 24 hours
			if (queue.count() == 144){queue.pop();}
			queue.enqueue( round(t) ); // round(t*10)/10
		}


}


int toggleSwitch(String args) {
    digitalWrite(RELAYPIN, relayState == 1 ? LOW : HIGH);
    return relayState = relayState == 1 ? 0 : 1;
}

int switchOn(String command) {
	pinMode(RELAYPIN, OUTPUT);
	digitalWrite(RELAYPIN, 0);
	relayState = 0;	
	override = true;
	return 0;}

int switchOff(String command) {
	pinMode(RELAYPIN, OUTPUT);
	digitalWrite(RELAYPIN, 1);
	relayState = 1;
	override = false;
	return 1;}	
	
	
int Switch(String args)
{
  // char id = args.charAt(0);

  if (args == "ON" || args == "on" || args == "1"){
    digitalWrite(RELAYPIN, LOW); 
    override = true;
    relayState = 0;
  }

  if (args == "OFF" || args == "off" || args == "0"){
    digitalWrite(RELAYPIN, HIGH);
    override = false;
    relayState = 1;
  }
  return relayState;
}

// answer is oposite of relaystate
int Status(String args) {
    int answer = 2;
    if (digitalRead(RELAYPIN) == 0) {
        relayState = 0;
        answer = 1;
    }
    if (digitalRead(RELAYPIN) == 1) {
        relayState = 1;
        answer = 0;
    }  
    //return relayState;
    return answer;
}


void update() {

    // calculate today's sunrise/sunset
    // minutes past midnight of sunrise (6 am would be 360)
    sunrise=mySunrise.Rise(Time.month(),Time.day()); // (month,day) - january=1
    sprintf(sr, "%02i:%02i", (int)mySunrise.sun_Hour(), (int)mySunrise.sun_Minute());
    sunset=mySunrise.Set(Time.month(),Time.day()); 
    sprintf(ss, "%02i:%02i", (int)mySunrise.sun_Hour(), (int)mySunrise.sun_Minute()); 
    
    lastUpdate = millis();
}

int getGraphData(String args) {

	strcpy(graphData,graph.toString());
	return 1;

}

void readDhtData() {
    
        /* analogvalue = analogRead(photoresistor);
        sprintf(p1, "%i", analogvalue);*/

        // Reading temperature or humidity takes about 250 milliseconds!
        // Sensor readings may also be up to 2 seconds 'old' (its a 
        // very slow sensor)
    	h = dht.getHumidity();
        // Read temperature as Celsius
    	t = dht.getTempCelcius();
        
        // Check if any reads failed and exit early (to try again).
    	if (isnan(h) || isnan(t)) {
    		// Serial.println("Failed to read from DHT sensor!");
    		// return;
    	} else {
    	    sprintf(h1, "%.0f", h); // convert Float to String
            sprintf(t1, "%.1f", t);
    	}        
        
        Particle.publish("temperature",t1);
        Particle.publish("humidity",h1);
        
        //Particle.publish("brightness",p1);
                
        // format your data as JSON, don't forget to escape the double quotes
        // sprintf(resultstr, "{\"current\":%i,\"sunrise\":%i,\"sunset\":%i}", current, sunrise, sunset);
        sprintf(resultstr, "{\"current\":\"%02i:%02i\",\"status\":\"%i\",\"sunrise\":\"%s\",\"sunset\":\"%s\",\"temp\":\"%.2f\",\"rh\":\"%.2f\"}", Time.hour(), Time.minute(), relayState, sr, ss, t, h);   
        
}