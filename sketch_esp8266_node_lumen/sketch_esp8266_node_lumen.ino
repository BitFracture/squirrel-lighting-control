/** 
 * The Flying Squirrels: Squirrel Lighting Controller
 * Node:     Lumen0 (WiFi Light 0)
 * Hardware: ESP8266-01[S] and MY9291 LED Driver
 * Purpose:  Allow controller to connect and set LED brightnesses
 * Author:   Erik W. Greif
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

//Custom libraries
#include <CommandInterpreter.h>
#include <TcpClientRegistrar.h>
 
const int LED_DATA_PIN = 13;
const int LED_CLCK_PIN = 15;
const char* WIFI_SSID = "SQUIRREL_NET";
const char* WIFI_PASS = "wj7n2-dx309-dt6qz-8t8dz";

my9291 ledDriver = my9291(LED_DATA_PIN, LED_CLCK_PIN, MY9291_COMMAND_DEFAULT);

//When iocontrol connects, it will be here
WiFiClient* clientIoControl = NULL;
CommandInterpreter serialCmd;
CommandInterpreter ioCmd;

//Allow connections and register clients
WiFiServer listenSocket(23);
TcpClientRegistrar clients;

bool reconnect = true;
bool colorCycle = false;
uint8_t colors[5];

WiFiEventHandler disconnectedEventHandler;

void setup() {
  
  //Initial light data
  colors[0] = 0;
  colors[1] = 0;
  colors[2] = 0;
  colors[3] = 255;
  colors[4] = 0;

  Serial.begin(9600);
  delay(500);
  Serial.print("Initialized\n");

  ledDriver.setColor((my9291_color_t) {colors[0], colors[1], colors[2], colors[3], colors[4]});
  ledDriver.setState(true);
  disconnectedEventHandler = WiFi.onStationModeDisconnected(&triggerReconnect);

  //Set up the wireless
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  //Assign some commands to the command controller
  serialCmd.assign("s", commandSetColors);
  serialCmd.assign("c", commandCycle);
  //serialCmd.assign("b", commandBinaryColors);
  ioCmd = CommandInterpreter(serialCmd);

  //Allow iocontrol to connect, give long timeout for now (debugging)
  clients.assign("iocontrol", &clientIoControl);
  clients.setConnectionTimeout(10000);

  //Start listening for control connection
  listenSocket.begin();
}

void triggerReconnect(const WiFiEventStationModeDisconnected& event) {
  
  reconnect = true;
}

void loop() {
  //Do nothing until we are connected to the server
  handleReconnect();
    
  //Handle incoming connections
  clients.handle(listenSocket);
  
  //Handle incoming commands
  serialCmd.handle(Serial);
  if (clientIoControl && clientIoControl->connected())
    ioCmd.handle(*clientIoControl);

  //If the auto mode is enabled, color cycle
  if (colorCycle) {
    float baseValue = (float)millis() / 3000.0f;
    int redCalc   = (sin(baseValue                           ) + (32.0f / 255.0f)) * 224.0f;
    int greenCalc = (sin(baseValue + ((2.0 * 3.14159) / 3.0f)) + (32.0f / 255.0f)) * 224.0f;
    int blueCalc  = (sin(baseValue + ((4.0 * 3.14159) / 3.0f)) + (32.0f / 255.0f)) * 224.0f;
    
    colors[0] = redCalc   > 0 ? (uint8_t)redCalc   : 0;
    colors[1] = greenCalc > 0 ? (uint8_t)greenCalc : 0;
    colors[2] = blueCalc  > 0 ? (uint8_t)blueCalc  : 0;
    colors[3] = 0;
    colors[4] = 0;
    ledDriver.setColor((my9291_color_t){colors[0], colors[1], colors[2], colors[3], colors[4]});
  }
}

void handleReconnect() {
  while (reconnect) {

    if (clientIoControl) {
      clientIoControl->stop();
      clientIoControl = NULL;
    }
    
    //Wait for wifi for 5 seconds
    Serial.print("Wait\n");
    for (int i = 10; WiFi.status() != WL_CONNECTED && i > 0; i--) {
      delay(500);
    }
    if (WiFi.status() != WL_CONNECTED) {
      break;
    }
  
    //Register this node with the controller
    WiFiClient clientSquirrel;
    if (!TcpClientRegistrar::connectClient(clientSquirrel, IPAddress(192, 168, 3, 1), 23, "lumen0", false)) {
      break;
    }
    
    reconnect = false;

    //Cycle colors to show connected
    ledDriver.setColor((my9291_color_t) { 255, 0, 0, 0 });
    delay(250);
    ledDriver.setColor((my9291_color_t) { 0, 255, 0, 0 });
    delay(250);
    ledDriver.setColor((my9291_color_t) { 0, 0, 255, 0 });
    delay(250);
    ledDriver.setColor((my9291_color_t) { 0, 0, 0, 255 });
    delay(250);
    ledDriver.setColor((my9291_color_t) { 255, 0, 0, 0 });
    delay(250);
    ledDriver.setColor((my9291_color_t) { 0, 255, 0, 0 });
    delay(250);
    ledDriver.setColor((my9291_color_t) { 0, 0, 255, 0 });
    delay(250);
    ledDriver.setColor((my9291_color_t) { 0, 0, 0, 255 });
    delay(250);
    ledDriver.setColor((my9291_color_t) {colors[0], colors[1], colors[2], colors[3], colors[4]});
  }
}

void commandCycle(Stream& port, int argc, const char** argv) {
  if (argc != 1) {
    port.print("ER\n");
    return;
  }

  colorCycle = argv[0][0] == '1';
  port.print("OK\n");
}

void commandSetColors(Stream& port, int argc, const char** argv) {

  if (argc < 3 || argc > 5) {
    port.print("ER\n");
    return;
  }
  
  colors[0] = hexToByte<uint8_t>(argv[0]);
  colors[1] = hexToByte<uint8_t>(argv[1]);
  colors[2] = hexToByte<uint8_t>(argv[2]);
  colors[3] = argc > 3 ? hexToByte<uint8_t>(argv[3]) : 0;
  colors[4] = argc > 4 ? hexToByte<uint8_t>(argv[4]) : 0;
  
  ledDriver.setColor((my9291_color_t){colors[0], colors[1], colors[2], colors[3], colors[4]});

  port.print("OK\n");
}

template <typename T> T hexToByte(const char* hexStr) {
  T output = 0;
  int numberOfBytes = strlen(hexStr);
  for (int i = 0; i < numberOfBytes; i++) {
    output <<= 4;
    
    if (hexStr[i] >= 'A' && hexStr[i] <= 'F')
      output |= static_cast<uint8_t>((hexStr[i] - 'A') + 10);
      
    else if (hexStr[i] >= 'a' && hexStr[i] <= 'f')
      output |= static_cast<uint8_t>((hexStr[i] - 'a') + 10);
      
    else if (hexStr[i] >= '1' && hexStr[i] <= '9')
      output |= static_cast<uint8_t>(hexStr[i] - '0');
  }
  
  return output;
}

