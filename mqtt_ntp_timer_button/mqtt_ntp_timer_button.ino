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

int updateInterval = 1000;//millisec.
unsigned int h;
unsigned int m;
unsigned int s;

#include <PubSubClient.h>

// Update these with values suitable for your network.
const char* ssid = "...";
const char* password = "...";
const char* mqtt_server = "...";

#define LEDPIN 13
#define RELAYPIN 12
#define BUTTONPIN 0     // the number of the pushbutton pin
int state = 0;
int workingMode = 1; // 1 = Auto, 0 = Manual

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
  }
  else {
    state = 0;
    digitalWrite(RELAYPIN, LOW);
    #ifdef LEDPIN
      digitalWrite(LEDPIN, HIGH);
    #endif
  }
}

void setup() {
  pinMode(LEDPIN, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  pinMode(RELAYPIN, OUTPUT);     
  Serial.begin(115200);

  inputEvent = t.every(1000, readInput);
  
  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  mqttEvent = t.every(1000, mqtt_job);  

  configTime(timezone, dst, "th.pool.ntp.org", "192.168.0.2");     //ดึงเวลาจาก Server
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

  if(strTopic == "Auto/Manual"){ //1 Auto , 0 = Manual
    if(strPayload == "1"){
      Serial.print("Mode : Auto");
      workingMode = 1;
    } else {
      Serial.print("Mode : Manual");
      workingMode = 0;
    }
  } else if(strTopic == "On/Off") { 
    if(strPayload == "1"){
      Serial.print("Manual : ON");
      updateIO(1);
    } else {
      Serial.print("Manual : OFF");
      updateIO(0);
    }
  } 
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("Auto/Manual");
      client.subscribe("On/Off");
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
  if(workingMode == 1){//Auto
    if((m%2) == 0){
      Serial.print("Auto : ON");
      updateIO(1);
    } else {
      Serial.print("Auto : OFF");
      updateIO(0); 
    }
  }

  if(h == 0 & m == 0){
    delay(60000);
    ESP.restart();
  }
}

void printTimeNow(){
  Serial.println();
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

  // check if the pushbutton is pressed.
  // if it is, the buttonState is HIGH:
  if (buttonState == LOW) {
    printTimeNow();
    state = (state == 0) ? 1 : 0;
    Serial.print("Button : ");
    Serial.print(state);
    updateIO(state);
    String tmpString = String(state);
    char pubString[2];
    tmpString.toCharArray(pubString, tmpString.length() + 1); //packaging up the data to publish to mqtt whoa...
    
    client.publish("On/Off", pubString);
//    delay(1000);
  }
}
