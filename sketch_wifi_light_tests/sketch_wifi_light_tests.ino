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
  ioCmd.assign("s", commandSetColors);

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
}

void commandSetColors(Stream& port, int argc, const char** argv) {

  if (argc != 4) {
    port.println("ER");
    return;
  }

  uint8_t red   = (uint8_t)atoi(argv[0]);
  uint8_t green = (uint8_t)atoi(argv[1]);
  uint8_t blue  = (uint8_t)atoi(argv[2]);
  uint8_t white = (uint8_t)atoi(argv[3]);
  ledDriver.setColor((my9291_color_t){red, green, blue, white});

  port.println("OK");
}

