/** 
 * The Flying Squirrels: Squirrel Lighting Controller
 * Node:     Lumen0 (WiFi Light 0)
 * Hardware: ESP8266-01[S] and MY9291 LED Driver
 * Purpose:  Allow controller to connect and set LED brightnesses
 * Author:   Erik W. Greifb
 * Date:     2017-10-26
 */

//ESP8266 libraries
#include <ESP8266WiFi.h>
#include <ESP8266WiFiAP.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFiScan.h>
#include <ESP8266WiFiSTA.h>
#include <ESP8266WiFiType.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>
#include <my9291.h>;
#include <my9231.h>;

//Custom libraries
#include <CommandInterpreter.h>

//Uncomment the hardware platform
//#define SONOFF_B1 0
#define THINKER_AILIGHT 1

#ifdef SONOFF_B1
const int LED_DATA_PIN = 12;
const int LED_CLCK_PIN = 14;
#elif THINKER_AILIGHT
const int LED_DATA_PIN = 13;
const int LED_CLCK_PIN = 15;
#endif
const char* WIFI_SSID = "SQUIRREL_NET";
const char* WIFI_PASS = "wj7n2-dx309-dt6qz-8t8dz";
const IPAddress broadcastAddress(192, 168, 3, 255);
const IPAddress serverAddress(   192, 168, 3, 1);
const int DEBUG_PORT = 24;
const int DATA_PORT = 23;

//Our definition of "warm" varies from platform to platform... how to do this?
#ifdef SONOFF_B1
const uint8_t warmColor[] = {  0,   0,  0,    0,   255};
#elif THINKER_AILIGHT
const uint8_t warmColor[] = {255, 128,  0,   64,   0};
#endif
const uint8_t coolColor[] = {  0,   0,  0,  255,   0};

#ifdef SONOFF_B1
my9231 ledDriver = my9231(LED_DATA_PIN, LED_CLCK_PIN, MY9291_COMMAND_DEFAULT);
#elif THINKER_AILIGHT
my9291 ledDriver = my9291(LED_DATA_PIN, LED_CLCK_PIN, MY9291_COMMAND_DEFAULT);
#endif

//When iocontrol connects, it will be here
WiFiUDP clientIoControl;
WiFiUDP broadcast; 
CommandInterpreter serialCmd;
CommandInterpreter ioCmd;

bool reconnect = true;
bool colorCycle = false;
uint8_t colors[5];

//Use for UDP send and receive
const int PACKET_DATA_SIZE = 64;
char packetData[PACKET_DATA_SIZE];

WiFiEventHandler disconnectedEventHandler;

void setup() {
  WiFi.persistent(false);
  
  //Initial light data
  colors[0] = 0;
  colors[1] = 0;
  colors[2] = 0;
  colors[3] = 255;
  colors[4] = 0;

  Serial.begin(9600);
  delay(500);
  Serial.print("Initialized\n");

  ledDriver.setState(true);
  ledDriver.setColor((my9291_color_t) {colors[0], colors[1], colors[2], colors[3], colors[4]});
  disconnectedEventHandler = WiFi.onStationModeDisconnected(&triggerReconnect);

  //Set up the wireless
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi ready to connect\n");

  //Assign some commands to the command controllers
  serialCmd.assign("c", commandSetColors);
  serialCmd.assign("t", commandSetTemp);
  ioCmd = CommandInterpreter(serialCmd);
}

void triggerReconnect(const WiFiEventStationModeDisconnected& event) {
  
  reconnect = true;
}

uint32_t lastComTime = 0;

void loop() {
  //Do nothing until we are connected to the server
  handleReconnect();

  //If we haven't heard from anyone in a while, throw out a discovery packet
  if (millis() - lastComTime > 5000) {
    
    //UDP Broadcast ourself!
    broadcast.beginPacket(broadcastAddress, DATA_PORT);
    broadcast.print("d"); //d = discover
    broadcast.endPacket();

    lastComTime = millis();
  }
  
  //Handle incoming commands (Serial)
  serialCmd.handle(Serial);

  //Handle UDP data stream (from iocontrol)
  ioCmd.handleUdp(clientIoControl);
}

void handleReconnect() {
  
  while (reconnect) {

    //Close the UDP data socket
    clientIoControl.stop();
    
    //Wait for wifi for 5 seconds
    Serial.print("Wait\n");
    for (int i = 10; WiFi.status() != WL_CONNECTED && i > 0; i--) {
      delay(500);
    }
    if (WiFi.status() != WL_CONNECTED) {
      break;
    }
    Serial.print("Connected\n");

    //Open udp port for lumen data
    clientIoControl.begin(DATA_PORT);
    
    reconnect = false;
    
    //UDP Broadcast this reset debug terminal output
    /*if (WiFi.localIP() == IPAddress(0,0,0,0)) {

      Serial.println("IP is blank... aborting send");
      reconnect = false;
    }
    else {
    
      sprintf(packetData, "debug \"Lumen node reset at %s\"\n", WiFi.localIP().toString().c_str());
      sendDebug(&packetData[0]);
    }*/
  }
}

/*void sendDebug(char* buff) {
  
  bool replied = false;

  while (!replied) {
    Serial.print("Sending debug packet...\n");
    
    //Send out debug data
    broadcast.beginPacket(serverAddress, DEBUG_PORT);
    broadcast.print(buff);
    broadcast.endPacket();

    //Get response from debug
    broadcast.begin(broadcast.localPort());
    int packetSize = 0;
    uint32_t timeout = 2000; 
    uint32_t timeStart = millis();
    while (((packetSize = broadcast.parsePacket()) <= 0) && (millis() - timeStart) < timeout) {
      delay(50);
    }
    if (packetSize > 0) {
      int len = broadcast.read(&packetData[0], PACKET_DATA_SIZE);
      packetData[len] = 0;
      Serial.printf("Got response %s\n", &packetData[0]);
      if (strcmp("OK\n", &packetData[0]) == 0)
        replied = true;
    }
    broadcast.stop();
  }
  
  //broadcast.stop();
}*/

void commandSetTemp(Stream& port, int argc, const char** argv) {

  lastComTime = millis();
  
  if (argc != 1 && argc != 2) {
    return;
  }

  //The multiplier defines where we are from cool to warm
  float multiplier = (float)atoi(argv[0]) / 255.0f;
  float brightness = argc == 2 ? (float)atoi(argv[1]) / 255.0f : 1.0f;

  for (int i = 0; i < 5; i ++) {
    float channelRaw = ((float)warmColor[i] + 
                ((float)coolColor[i] - 
                (float)warmColor[i]) * multiplier);
    colors[i] = (uint8_t)(channelRaw * brightness);
  }

  ledDriver.setColor((my9291_color_t){colors[0], colors[1], colors[2], colors[3], colors[4]});
}

void commandSetColors(Stream& port, int argc, const char** argv) {

  lastComTime = millis();

  if (argc < 3 || argc > 5) {
    return;
  }

  colors[0] = (uint8_t)atoi(argv[0]);
  colors[1] = (uint8_t)atoi(argv[1]);
  colors[2] = (uint8_t)atoi(argv[2]);
  colors[3] = argc > 3 ? (uint8_t)atoi(argv[3]) : 0;
  colors[4] = argc > 4 ? (uint8_t)atoi(argv[4]) : 0;
  
  ledDriver.setColor((my9291_color_t){colors[0], colors[1], colors[2], colors[3], colors[4]});
}


