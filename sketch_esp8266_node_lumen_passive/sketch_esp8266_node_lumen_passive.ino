/** 
 * The Flying Squirrels: Squirrel Lighting Controller
 * Node:     Lumen (WiFi Light)
 * Hardware: ESP8266EX and MY9291/MY9231 LED Drivers
 * Purpose:  Connect to a network and receive light control data
 * Author:   Erik W. Greif
 * Date:     2018-06-19
 * 
 * ROM sizes:
 *   Thinker AI Light: 1MB, 64KB SPIFFS
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
#include <DNSServer.h>
#include <ESP8266WebServer.h>

//Custom libraries
#include <CommandInterpreter.h>
#include "Persistence.h"

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
Persistence persistence;
IPAddress broadcastAddress(192, 168, 3, 255);
const IPAddress pairingIp(1, 1, 1, 1);

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
WiFiUDP clientData;
WiFiUDP broadcast; 
CommandInterpreter serialCmd;
CommandInterpreter dataCmd;

const uint16_t DNS_PORT = 53;
const uint16_t HTTP_SERVER_PORT = 80;
ESP8266WebServer webServer(HTTP_SERVER_PORT);
DNSServer dnsServer;

bool reconnect = true;
bool colorCycle = false;
uint8_t colors[5];

const uint8_t PAIR_CYCLE_COUNT = 2;
const bool BOOT_PAIR = 1;
const bool BOOT_NORMAL = 0;
bool bootMode = BOOT_NORMAL;
uint32_t cycleClearTime = 0;

//Use for UDP send and receive
const int PACKET_DATA_SIZE = 64;
char packetData[PACKET_DATA_SIZE];

WiFiEventHandler disconnectedEventHandler;

void setup() {
  WiFi.persistent(false);
  Persistence::init();
  persistence = Persistence::load();

  //Increment the cycle count and dump to flash
  uint8_t cycles = persistence.incrementAndGetCycles();
  if (cycles >= PAIR_CYCLE_COUNT) {
    bootMode = BOOT_PAIR;
    persistence.resetCycles();
  }
  persistence.dump();
  cycleClearTime = millis() + 4000;
  
  //Initial light data
  colors[0] = 0;
  colors[1] = 0;
  colors[2] = 0;
  colors[3] = 128;
  colors[4] = 128;

  Serial.begin(9600);
  delay(500);

  switch (bootMode) {
    case BOOT_NORMAL: setupNormal(); break;
    case BOOT_PAIR: setupPair(); break;
  }
}

void setupNormal() {
  Serial.print("Booting normal mode\n");
  
  ledDriver.setState(true);
  ledDriver.setColor((my9291_color_t) {colors[0], colors[1], colors[2], colors[3], colors[4]});
  disconnectedEventHandler = WiFi.onStationModeDisconnected(&triggerReconnect);

  //Set up the wireless
  WiFi.mode(WIFI_STA);
  WiFi.begin(persistence.getSsid(), persistence.getPass());
  Serial.print("WiFi ready to connect\n");

  //Assign some commands to the command controllers
  serialCmd.assign("c", commandSetColors);
  serialCmd.assign("t", commandSetTemp);
  dataCmd = CommandInterpreter(serialCmd);
}

void setupPair() {
  Serial.print("Booting pairing mode\n");

  ledDriver.setState(true);
  ledDriver.setColor((my9291_color_t) {0, 0, 0, 0, 0});

  //Set up the access point
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(250);
  
  WiFi.softAPConfig(pairingIp, pairingIp, IPAddress(255, 255, 255, 0));
  char* ssid = "BitLight-0000";
  memcpy(ssid + 9, persistence.getTransientId(), 4);
  if (!WiFi.softAP(ssid)) {
    Serial.printf("Access point \"%s\" could not initialize\n", ssid);
    ESP.restart();
    return;
  }
  
  dnsServer.start(DNS_PORT, "*", pairingIp);
  webServer.onNotFound(onWebRequest);
  webServer.begin();
  
  Serial.print("WiFi is ready to accept connections\n");
}

void triggerReconnect(const WiFiEventStationModeDisconnected& event) {
  
  reconnect = true;
}

uint32_t lastComTime = 0;
uint32_t lastAddrCheckTime = 0;

void loop() {
  switch (bootMode) {
    case BOOT_NORMAL: loopNormal(); break;
    case BOOT_PAIR: loopPair(); break;
  }
}

void loopNormal() {
  //Do nothing until we are connected to the server
  handleReconnect();

  //Clear the reset counter if we pass reset cycle timeout
  if (cycleClearTime && millis() > cycleClearTime) {
    cycleClearTime = 0;
    persistence.resetCycles();
    persistence.dump();
  }

  //Update broadcast IP every second or so as long as we aren't actively receiving
  if (millis() - lastAddrCheckTime > 1000) {
    IPAddress mine = WiFi.localIP();
    IPAddress mask = WiFi.subnetMask();
    broadcastAddress = IPAddress(
        mine[0] | (~mask[0] & 255), 
        mine[1] | (~mask[1] & 255), 
        mine[2] | (~mask[2] & 255), 
        mine[3] | (~mask[3] & 255));
    Serial.printf("IP %s, subnet %s, broadcast %s\n", 
        mine.toString().c_str(), mask.toString().c_str(), 
        broadcastAddress.toString().c_str());
    lastAddrCheckTime = millis();
  }
  //If we haven't heard from anyone in a while, throw out a discovery packet
  if (millis() - lastComTime > 5000) {
    
    //UDP Broadcast ourself!
    broadcast.beginPacket(broadcastAddress, persistence.getPort());
    broadcast.print("d"); //d = discover
    broadcast.endPacket();

    lastComTime = millis();
    lastAddrCheckTime = millis();
  }
  
  //Handle incoming commands (Serial)
  serialCmd.handle(Serial);

  //Handle UDP data stream (from iocontrol)
  dataCmd.handleUdp(clientData);
}

void loopPair() {
  delay(100);
  dnsServer.processNextRequest();
  webServer.handleClient();
}

void onWebRequest() {
  String uri = webServer.uri();
  if (uri.equals("/authenticate")) {
    Serial.println("Web request received");
    onAuthRequest();
  } else {
    webServer.sendHeader("Location", "http://bit.bulb/authenticate");
    webServer.send(302, "text/html", "");
  }
}

void onAuthRequest() {
  if (webServer.method() == HTTP_GET) {
    webServer.send(200, "text/html", 
          "<!DOCTYPE html>\n<html>"
          "<head>"
              "<style>"
                "div.container {max-width: 400px; margin-left: auto; margin-right: auto;"
                "    background-color: #FFFFFF; padding: 24px; border-radius: 16px; }"
                "body {background-color: #555555;}"
                "input[type=text] {border: .1em solid #000000;}"
                "input[type=password] {border: .1em solid #000000;}"
                "p, label, input {font-size: 20px !important;}"
                "h1 {font-size: 48px;}"
                "h2 {font-size: 32px;}"
              "</style>"
              "<meta name=\"viewport\" content=\"width=500, target-densitydpi=device-dpi\">"
          "</head>"
          "<body>"
              "<div class=\"container\">"
                  "<h1>BitLight</h1><h2>Connect To Your Network</h2>"
                  "<p>In order to use this BitLight, "
                  "it must connect to your local wireless network. This is probably the "
                  "same network you use every day.<br/>Enter that information here "
                  "and submit the form.</p>"
                  "<form method=\"post\">"
                      "<label>Wi-Fi Name (SSID)</label><br/>"
                      "<input name=\"ssid\" type=\"text\" value=\"\"></input><br/>"
                      "<label>Wi-Fi Password</label><br/>"
                      "<input name=\"pass\" type=\"password\" value=\"\"></input><br/><br/>"
                      "<input type=\"submit\"></input>"
                  "</form>"
              "</div>"
          "</body></html>");
  }

  //Form submission
  else if (webServer.method() == HTTP_POST) {
    String ssid = webServer.arg("ssid");
    String pass = webServer.arg("pass");
    bool failed = false;
    if (ssid.length() <= 1 || ssid.length() > 32)
      failed = true;
    if (pass.length() > 32)
      failed = true;

    //If failure, report it
    if (failed) webServer.send(200, "text/html", 
          "<!DOCTYPE html>\n<html>"
          "<head>"
              "<style>"
                "div.container {max-width: 400px; margin-left: auto; margin-right: auto;"
                "    background-color: #FFFFFF; padding: 24px; border-radius: 16px; }"
                "body {background-color: #555555;}"
                "input[type=text] {border: .1em solid #000000;}"
                "input[type=password] {border: .1em solid #000000;}"
                "p, label, input {font-size: 20px !important;}"
                "h1 {font-size: 48px;}"
                "h2 {font-size: 32px;}"
              "</style>"
              "<meta name=\"viewport\" content=\"width=500, target-densitydpi=device-dpi\">"
          "</head>"
          "<body>"
              "<div class=\"container\">"
                  "<h1>BitLight</h1><h2>Something is wrong...</h2>"
                  "<p>Your network name and password may not be more than 32 characters. "
                  "Your network name may not be empty. These are the same credentials you use "
                  "to connect your phone or personal computer to your network.</p><br/>"
                  "<a href=\"/\">Try Again</a>"
              "</div>"
          "</body></html>");
      
    if (!failed) webServer.send(200, "text/html", 
          "<!DOCTYPE html>\n<html>"
          "<head>"
              "<style>"
                "div.container {max-width: 400px; margin-left: auto; margin-right: auto;"
                "    background-color: #FFFFFF; padding: 24px; border-radius: 16px; }"
                "body {background-color: #555555;}"
                "input[type=text] {border: .1em solid #000000;}"
                "input[type=password] {border: .1em solid #000000;}"
                "p, label, input {font-size: 20px !important;}"
                "h1 {font-size: 48px;}"
                "h2 {font-size: 32px;}"
              "</style>"
              "<meta name=\"viewport\" content=\"width=500, target-densitydpi=device-dpi\">"
          "</head>"
          "<body>"
              "<div class=\"container\">"
                  "<h1>BitLight</h1><h2>Thank you</h2>"
                  "<p>Your BitLight is restarting and will try to connect "
                  "to your network. If the bulb does not connect to your controller, try "
                  "these connection steps again, as this may be a sign that you have typed "
                  "your network settings incorrectly. If your network is an AC "
                  "(5GHz band) network, you must enable the B, G, or N wireless network "
                  "to connect this device.</p>"
              "</div>"
          "</body></html>");

    //Update network settings, exit pair mode
    if (!failed) {
      persistence.setSsid((char*)ssid.c_str());
      persistence.setPass((char*)pass.c_str());
      persistence.dump();
      ESP.restart();
    }
  }
}

void handleReconnect() {
  
  while (reconnect) {

    //Close the UDP data socket
    clientData.stop();
    
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
    clientData.begin(persistence.getPort());
    
    reconnect = false;
  }
}

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


