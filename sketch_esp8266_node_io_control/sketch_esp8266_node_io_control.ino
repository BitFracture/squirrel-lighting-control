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

#include <TcpClientRegistrar.h>

//Pcf8591 ioChip(&Wire);
WiFiClient clientSquirrel;
WiFiClient clientLumen;
WiFiUDP broadcast;

const char* WIFI_SSID = "SQUIRREL_NET";
const char* WIFI_PASS = "wj7n2-dx309-dt6qz-8t8dz";
const IPAddress BROADCAST_ADDR(192, 168, 3, 100);
bool reconnect = true;
long lastCheckTime = 0;

WiFiEventHandler disconnectedEventHandler;

void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.print("Initialized\n");

  //Set up the wireless
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  disconnectedEventHandler = WiFi.onStationModeDisconnected(&triggerReconnect);
}

uint32_t startTime, endTime;
uint32_t baseTime = 0;

void loop() {
  static uint8_t rLumen, gLumen, bLumen;
  static uint32_t lastTime, thisTime;
  
  //Do nothing until we are connected to the server
  handleReconnect();

  //Rate limit the UDP packets to about 30FPS/PPS
  thisTime = millis();
  if (thisTime - lastTime > 32) {
    lastTime = thisTime;
    
    //Construct data
    rLumen = (uint8_t)((sin((millis() / 1000.0f) + 6.28f / 3    ) + 1.0f) * 127.0f);
    gLumen = (uint8_t)((sin((millis() / 1000.0f) + 6.28f / 3 * 2) + 1.0f) * 127.0f);
    bLumen = (uint8_t)((sin((millis() / 1000.0f) + 0            ) + 1.0f) * 127.0f);
    
    char* toSend = "s 00 00 00\n";
    byteToString(rLumen, toSend + 2);
    byteToString(gLumen, toSend + 5);
    byteToString(bLumen, toSend + 8);
  
    //TEST: Try the UDP broadcast method
    broadcast.beginPacket(BROADCAST_ADDR, 23);
    broadcast.write(toSend);
    broadcast.endPacket();
  }

  //Send the light(s) some test data!
  /*if (clientLumen.connected()) {
    uint8_t rLumen = (uint8_t)((sin((millis() / 1000.0f) + 6.28f / 3    ) + 1.0f) * 127.0f);
    uint8_t gLumen = (uint8_t)((sin((millis() / 1000.0f) + 6.28f / 3 * 2) + 1.0f) * 127.0f);
    uint8_t bLumen = (uint8_t)((sin((millis() / 1000.0f) + 0            ) + 1.0f) * 127.0f);
    
    char* toSend = "s 00 00 00\n";
    byteToString(rLumen, toSend + 2);
    byteToString(gLumen, toSend + 5);
    byteToString(bLumen, toSend + 8);

    clientLumen.flush();
    clientLumen.print(toSend);
    String response = clientLumen.readStringUntil('\n');
    
    if (!response.equals("OK")) {
      Serial.print("Bad response, ");
      if (TcpClientRegistrar::probeConnection(clientLumen)) {
        Serial.print("probe succeeded\n");
      } else {
        Serial.print("probe FAILED\n");
        clientLumen.stop();
      }
    }
  }
  //Connect to the light
  else if (millis() - lastCheckTime > 1000) {
    lastCheckTime = millis();
    Serial.print("Attempt Lumen0 connect\n");
    
    //Connect to the bulb
    clientSquirrel.flush();
    clientSquirrel.print("ip lumen0\n");
    String response = clientSquirrel.readStringUntil('\n');
    
    IPAddress lumenAddress;
    if (lumenAddress.fromString(response) && lumenAddress != 0) {
      if (TcpClientRegistrar::connectClient(clientLumen, lumenAddress, 23, "iocontrol", true)) {
        clientLumen.setNoDelay(false);
        clientLumen.setTimeout(1000);
      }
    } else {
      Serial.print("INVALID IP\n");
    }
  }*/
}

void triggerReconnect(const WiFiEventStationModeDisconnected& event) {

  reconnect = true;
}

void handleReconnect() {

  //TODO: Reconnect if server TCP connection lost
  if (!clientSquirrel.connected())
    reconnect = true;
  
  while (reconnect) {

    //Clean up all connections
    clientSquirrel.stop();
    clientLumen.stop();
    broadcast.stop();
    
    //Wait for wifi for 5 seconds
    Serial.print("Wait\n");
    for (int i = 10; WiFi.status() != WL_CONNECTED && i > 0; i--) {
      delay(500);
    }
    if (WiFi.status() != WL_CONNECTED) {
      continue;
    }

    //Open broadcast port for lumen data
    broadcast.begin(23);

    //Try to connect persistently to squirrel
    if (TcpClientRegistrar::connectClient(
	        clientSquirrel, IPAddress(192, 168, 3, 1), 23, "iocontrol", true))
      reconnect = false;
  }
}

/**
 * Overwrites the first two characters with the hex equivalent of the byte given.
 */
void byteToString(uint8_t toConvert, char* writeStart) {
  static char* charLookup = "0123456789ABCDEF";
  
  writeStart[1] = charLookup[ (toConvert       & 15)];
  writeStart[0] = charLookup[((toConvert >> 4) & 15)];
}

