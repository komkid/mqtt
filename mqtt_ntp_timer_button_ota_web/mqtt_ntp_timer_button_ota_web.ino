#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

#include <ESP8266WiFi.h>
#include <time.h>
int timezone = 7 * 3600;                    
int dst = 0;                                
struct tm* p_tm;

#include "Timer.h"
Timer t;
int inputEvent;
int timeEvent;
int mqttEvent;

int updateInterval = 50000;//millisec.
unsigned int h;
unsigned int m;
unsigned int s;

#include <PubSubClient.h>

// Update these with values suitable for your network.
//################################################
const char* ssid = "...";
const char* password = "...";
const char* mqtt_server = "...";
const char* mqtt_user = "...";
const char* mqtt_pass = "...";
const char* myThing = "SONOFF-CLIENT";
const char* myTopic = "OnOff";
//################################################

#define LEDPIN 13
#define RELAYPIN 12
#define BUTTONPIN 0     // the number of the pushbutton pin
int state = 1;
//int workingMode = 1; // 1 = Auto, 0 = Manual

WiFiClient espClient;
PubSubClient client(espClient);

void blinking(int qty=5){
  int n;
  for (n = 1; n < qty ; n++){
    digitalWrite(LEDPIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
    delay(200);                      // Wait for a second
    digitalWrite(LEDPIN, HIGH);  // Turn the LED off by making the voltage HIGH
    delay(200);                      // Wait for two seconds (to demonstrate the active low LED)
  }
}

void updateIO(int state) {
  if (state == 1) {
    digitalWrite(RELAYPIN, HIGH);
    #ifdef LEDPIN
      digitalWrite(LEDPIN, LOW);
    #endif
  } else {
    digitalWrite(RELAYPIN, LOW);
    #ifdef LEDPIN
      digitalWrite(LEDPIN, HIGH);
    #endif
  }
}

void printTimeNow(){
  Serial.println();
  Serial.print(WiFi.localIP());
  Serial.print(" >> ");
  Serial.print(h); // print the hour (86400 equals secs per day)
  Serial.print(':');
  if ( m < 10 ) {
    // In the first 10 minutes of each hour, we'll want a leading '0'
    Serial.print('0');
  }
  Serial.print(m); // print the minute (3600 equals secs per minute)
  Serial.print(':');
  if ( s < 10 ) {
    // In the first 10 seconds of each minute, we'll want a leading '0'
    Serial.print('0');
  }
  Serial.print(s); // print the second
  Serial.print(" >> "); // print the second
}

void readInput()
{
  // read the state of the pushbutton value:
  int buttonState = digitalRead(BUTTONPIN);

  if (buttonState == LOW) {
    state = (state == 0) ? 1 : 0;
    printTimeNow();
    if(state == 0){
      Serial.print("Button : 0");
      updateIO(0);
      client.publish(myTopic, "0");
    } else {
      Serial.print("Button : 1");
      updateIO(1);
      client.publish(myTopic, "1");
    }
  }
}

void setup() {
  pinMode(LEDPIN, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  pinMode(RELAYPIN, OUTPUT);     
  Serial.begin(115200);
  updateIO(1);

  inputEvent = t.every(1000, readInput);
  
  setup_wifi();
  
  MDNS.begin(myThing);

  httpUpdater.setup(&httpServer);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  mqttEvent = t.every(1000, mqtt_job);  

  configTime(timezone, dst, "th.pool.ntp.org", "192.168.0.2");
  Serial.println("\nWaiting for time");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
  time_t now = time(nullptr);
  p_tm = localtime(&now);
  h = p_tm->tm_hour;
  m = p_tm->tm_min;
  s = p_tm->tm_sec;

  timeEvent = t.every(updateInterval, tikTok);  

  printTimeNow();
  Serial.println("Now");
  blinking(5);
  updateIO(1);
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}



void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String strPayload = String((char*)payload);
  String strTopic = String((char*)topic);

  printTimeNow();
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  Serial.print(strPayload);
  Serial.println();

  if(strTopic == myTopic) { 
    if(strPayload == "1"){
      Serial.print("MQTT : ON");
      updateIO(1);
    } else {
      Serial.print("MQTT : OFF");
      updateIO(0);
    }
  } 
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(myThing, mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish("outTopic", "hello world");
      // ... and resubscribe
      //client.subscribe("Auto/Manual");
      client.subscribe(myTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void mqtt_job(){
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

void loop() {
  httpServer.handleClient();
  t.update();
}

void tikTok(){
  s = s + (updateInterval / 1000);
  if((s / 60) > 0) {
    s = s % 60; 
    m++;
    if(m == 60){
      m = 0;
      h++;
      if(h == 24) h = 0;
    }
  }
  printTimeNow();
  if(m%2 == 0){
    Serial.print("AUTO : OFF");
    updateIO(0);
    client.publish(myTopic, "0");
  } else {
    Serial.print("AUTO : ON");
    updateIO(1);
    client.publish(myTopic, "1");
  }
}
