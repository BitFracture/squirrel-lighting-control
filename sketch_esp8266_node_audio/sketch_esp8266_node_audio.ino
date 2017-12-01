/**
   The Flying Squirrels: Squirrel Lighting Controller
   Node:     2/Client: Sun Meter
   Hardware: ESP8266-01[S]
   Purpose:  Monitor exterior light levels, report to server.
   Author:   Erik W. Greif
   Date:     2017-10-14
*/

#define CLAP_THRESHHOLD    10

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

#include <TcpClientRegistrar.h>
#include <CommandInterpreter.h>
#include <Pcf8591.h>
#include "AverageTracker.h"

Pcf8591 ioChip(&Wire);

const char* WIFI_SSID = "SQUIRREL_NET";
const char* WIFI_PASS = "wj7n2-dx309-dt6qz-8t8dz";
bool reconnect = true;

void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.print("Initialized\n");
  Wire.begin(2, 0);

  WiFi.mode(WIFI_STA);
}

AverageTracker<uint8_t> avg(14);

void loop() {
//*
  //Capture audio level from chip
  static uint8_t level = 0, lastLevel = 0, lastMaxLevel = 0;
  static uint32_t thisSampleTime, lastSampleTime = 0;
  static unsigned long lastClapThreshHoldTime = 0;

  level = ioChip.read(0, 0);
  avg.add(level);
  
  if (level <= lastLevel) {
    if (lastMaxLevel > avg.average() + CLAP_THRESHHOLD) {
      if (millis() - lastClapThreshHoldTime < 500) {
        Serial.println("Peak Detected ----------------------");
        
        // Clap Peak detected
        ioChip.write(0, 255, !ioChip.getOutputEnabled());
        lastClapThreshHoldTime = 0;
      } else {
        lastClapThreshHoldTime = millis();
      }

      lastMaxLevel = -1;
      Serial.println("-- Down Peak --");
    }
    
    lastMaxLevel = 0;
  } else {
    lastMaxLevel = level;
  }

  lastLevel = level;

  for (int i = avg.average(); i > 0; i--)
    Serial.print("*");

  Serial.print("[");
  Serial.print(level);
  Serial.println("]");
  
  //**/
}

/**
 * Blinks the output LED at the given rate.
 */
void handleHeartbeat() {
  static uint32_t aliveIndicateTime = 0;
  
  //Blink the LED on AOut by toggling from output to hi-z mode
  if (millis() - aliveIndicateTime > 2000) {
    aliveIndicateTime = millis();
    ioChip.write(0, 255, !ioChip.getOutputEnabled());
  }
}

void triggerReconnect(const WiFiEventStationModeDisconnected& event) {

  reconnect = true;
}

void handleReconnect() {
  //TODO: Reconnect if server TCP connection lost
  while (reconnect) {
    //Wait for wifi for 5 seconds
    Serial.print("Wait\n");
    for (int i = 10; WiFi.status() != WL_CONNECTED && i > 0; i--) {
      delay(500);
    }
    if (WiFi.status() != WL_CONNECTED) {
      continue;
    }

    //Try to connect persistently to squirrel
    WiFiClient registerClient;
    if (TcpClientRegistrar::connectClient(
          registerClient, IPAddress(192, 168, 3, 1), 23, "pressure", false))
      reconnect = false;
  }
}

void getPressure(Stream& reply, int argc, const char** argv) {
  static const int BUFFER_LEN = 5;
  static char buffer[BUFFER_LEN];
  sprintf(buffer, "%i\n", ioChip.read(0, 0));
  reply.print(buffer);
}

