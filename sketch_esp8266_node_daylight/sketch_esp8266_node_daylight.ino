/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Node:     2/Client: Sun Meter
 * Hardware: ESP8266-01[S]
 * Purpose:  Monitor exterior light levels, report to server.
 * Author:   Erik W. Greif
 * Date:     2017-10-14
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

#include <CommandInterpreter.h>

const char* WIFI_SSID = "SQUIRREL_NET";
const char* WIFI_PASS = "wj7n2-dx309-dt6qz-8t8dz";

void setup() {

  Serial.print("DEBUG: Connecting to WiFi access point");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nDEBUG: WiFi is connected");
}

void loop() {
}
