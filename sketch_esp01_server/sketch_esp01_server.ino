/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Node:     1/Server
 * Hardware: ESP8266-01[S]
 * Purpose:  Interface user commands, act as wireless router.
 * Author:   Erik W. Greif
 * Date:     2017-10-13
 */

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
#include "CommandInterpreter.h"
#include "TcpClientRegistrar.h"

const char* WIFI_SSID = "SQUIRREL_NET";
const char* WIFI_PASS = "wj7n2-dx309-dt6qz-8t8dz";
IPAddress localIp(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

//TCP connection pointers
WiFiClient* clientMobile    = NULL;
WiFiClient* clientDaylight  = NULL;
WiFiClient* clientIoControl = NULL;

//Interpreters for user connections (clones, w/ separate buffers)
CommandInterpreter serialCmd;
CommandInterpreter mobileCmd;

//TCP server and the client id registrar: Handle reconnects seamlessly
WiFiServer listenSocket(23);
TcpClientRegistrar clients;

void setup() {
  //Get wi-fi connected
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(localIp, gateway, subnet);
  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  
  delay(500);
  Serial.begin(9600);
  Serial.println("DEBUG: WiFi AP is ready");
  
  listenSocket.begin();
  Serial.println("DEBUG: Server is ready");

  //Register user commands to handler functions
  serialCmd.assignDefault(commandNotFound);
  serialCmd.assign("diag", commandGetDiagnostics);
  serialCmd.assign("ip", commandGetIp);
  serialCmd.assign("scan", commandScanNetworks);
  serialCmd.assign("identify", commandIdentify);
  
  //Allow mobile to do everything that the serial term can (copy)
  mobileCmd = CommandInterpreter(serialCmd);

  //Register client IDs to respective pointers for auto connection handling
  clients.assign("mobile", &clientMobile);
  clients.assign("daylight", &clientDaylight);
  clients.assign("iocontrol", &clientIoControl);
}

void loop() {
  //Handle incoming TCP connections
  clients.handle(listenSocket);
  
  //Handle dispatching commands from various sources if they are available
  serialCmd.handle(Serial);
  if (clientMobile)
    mobileCmd.handle(*clientMobile);

  //Slow things down a bit
  delay(5);
}

void commandNotFound(Stream& port) {
  port.println("Unknown command");
}

void commandGetDiagnostics(Stream& port) {
  WiFi.printDiag(port);
}

void commandGetIp(Stream& port) {
  port.println(WiFi.softAPIP());
}

void commandScanNetworks(Stream& port) {
  int numNetworks = WiFi.scanNetworks();
  for (int i = 0; i < numNetworks; i++) {
    port.print(WiFi.SSID(i));
    port.print(" (");
    port.print(WiFi.RSSI(i));
    port.print(", ");
    port.print(WiFi.channel(i));
    port.println(")");
  }
}

void commandIdentify(Stream& port) {
  port.println("squirrel");
}



