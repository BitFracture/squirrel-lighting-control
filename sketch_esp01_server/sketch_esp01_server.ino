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

const char* WIFI_SSID = "SQUIRREL_NET";
const char* WIFI_PASS = "wj7n2-dx309-dt6qz-8t8dz";

WiFiClient* clientMobile = NULL;
CommandInterpreter userInput;

WiFiServer listenSocket(23);

void setup() {
  //Get wi-fi connected
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  
  delay(500);
  Serial.begin(9600);
  Serial.println("DEBUG: WiFi AP is ready");
  
  listenSocket.begin();
  Serial.println("DEBUG: Server is ready");

  //Register user commands to handler functions
  //userInput.setPrefix('/');
  userInput.assignDefault(commandNotFound);
  userInput.assign("diag", commandGetDiagnostics);
  userInput.assign("ip", commandGetIp);
  userInput.assign("scan", commandScanNetworks);
}

void loop() {
  userInput.handle(Serial);
  if (clientMobile)
    userInput.handle(*clientMobile);
  delay(50);

  //Handle new wireless connections
  WiFiClient newClient = listenSocket.available();
  if (newClient) {
    while (!newClient.connected())
      delay(1);
    newClient.setTimeout(5000);
    newClient.println("identify");
    String clientId = newClient.readStringUntil('\n');
    clientId.replace("\r", "");
    Serial.print("DEBUG: Client incoming with identity=\"");
    Serial.print(clientId);
    Serial.println("\"");

    if (clientId.equals("mobile")) {
      if (clientMobile) {
        Serial.println("DEBUG: Connection already open, closing existing connection");
        clientMobile->stop();
        delete clientMobile;
      }
      clientMobile = new WiFiClient(newClient);
    }
    else {
      newClient.stop();
    }
  }
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



