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

my9291 ledDriver = my9291(
    LED_DATA_PIN, 
    LED_CLCK_PIN, 
    MY9291_COMMAND_DEFAULT
);

//When iocontrol connects, it will be here
WiFiClient* clientIoControl = NULL;
CommandInterpreter serialCmd;
CommandInterpreter ioCmd;

//Allow connections and register clients
WiFiServer listenSocket(23);
TcpClientRegistrar clients;

bool colorCycle = false;

void setup() {
  Serial.begin(9600);
  delay(500);

  //Set up the wireless
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while(WiFi.status() != WL_CONNECTED) {
    Serial.println("Waiting for connection...");
    delay(5000);
  }

  delay(500);
  Serial.println("Squirrel SmartLight firmware initialized");

  //Register this node with the controller
  Serial.print("Registering with controller");
  WiFiClient registerConnection;
  registerConnection.connect(IPAddress(192, 168, 1, 1), 23);
  while (!registerConnection.connected()) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nSuccess!");

  yield();
  String cmd = registerConnection.readStringUntil('\n');
  if (!cmd.equals("mode"))
    registerConnection.stop();
  else {
    registerConnection.println("register");
    cmd = registerConnection.readStringUntil('\n');
    if (!cmd.equals("identify"))
      registerConnection.stop();
    else {
      registerConnection.println("lumen0");
      registerConnection.stop();
    }
  }

  //Assign some commands to the command controller
  serialCmd.assign("s", commandSetColors);
  serialCmd.assign("c", commandCycle);
  ioCmd = CommandInterpreter(serialCmd);

  //Allow iocontrol to connect, give long timeout for now (debugging)
  clients.assign("iocontrol", &clientIoControl);
  clients.setConnectionTimeout(10000);

  //Start listening for control connection
  listenSocket.begin();

  //Set some initial LED color (all red)
  ledDriver.setColor((my9291_color_t) { 255, 0, 0, 0 });
  ledDriver.setState(true);
}

void loop() {
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
    
    uint8_t red   = redCalc   > 0 ? (uint8_t)redCalc   : 0;
    uint8_t green = greenCalc > 0 ? (uint8_t)greenCalc : 0;
    uint8_t blue  = blueCalc  > 0 ? (uint8_t)blueCalc  : 0;
    uint8_t white = 0;
    ledDriver.setColor((my9291_color_t){red, green, blue, white});
  }
}

void commandCycle(Stream& port, int argc, const char** argv) {
  if (argc != 1) {
    port.println("ER");
    return;
  }

  colorCycle = argv[0][0] == '1';
  port.println("OK");
}

void commandSetColors(Stream& port, int argc, const char** argv) {

  if (argc != 4) {
    port.println("ER");
    return;
  }

  ledDriver.setColor((my9291_color_t){(uint8_t)atoi(argv[0]),
    (uint8_t)atoi(argv[1]),
    (uint8_t)atoi(argv[2]),
    (uint8_t)atoi(argv[3])});

  port.println("OK");
}

