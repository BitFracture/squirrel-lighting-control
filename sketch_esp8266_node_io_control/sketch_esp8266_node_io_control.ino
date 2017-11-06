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
#include <CommandInterpreter.h>
#include <Pcf8591.h>

Pcf8591 ioChip(&Wire);
WiFiClient clientSquirrel;
WiFiUDP clientDiscover;
WiFiUDP dataBroadcast;

const char* WIFI_SSID = "SQUIRREL_NET";
const char* WIFI_PASS = "wj7n2-dx309-dt6qz-8t8dz";
bool reconnect = true;
long lastCheckTime = 0;

const int MAX_LUMEN_NODES = 16;
IPAddress lumenNodes[MAX_LUMEN_NODES];

WiFiEventHandler disconnectedEventHandler;

void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.print("Initialized\n");
  Wire.begin(2, 0);

  //Set up the wireless
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  disconnectedEventHandler = WiFi.onStationModeDisconnected(&triggerReconnect);
}

void loop() {
  static uint8_t rLumen, gLumen, bLumen;
  static uint32_t lastSendTime, thisSendTime, lastSampleTime, thisSampleTime;
  
  //Do nothing until we are connected to the server
  handleReconnect();

  //Catch any discovery packets from lumen nodes ("d")
  char discBuffer;
  int packetSize = clientDiscover.parsePacket();
  if (packetSize) {
    
    discBuffer = 0;
    IPAddress newClientIP = clientDiscover.remoteIP();
    
    clientDiscover.read(&discBuffer, 1);
    if (discBuffer == 'd') {

      int i = 0;
      for (; i < MAX_LUMEN_NODES && lumenNodes[i] != 0 && lumenNodes[i] != newClientIP; i++);
      if (i < MAX_LUMEN_NODES && lumenNodes[i] != newClientIP) {
        Serial.print("Lumen node ");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(newClientIP);
        Serial.print('\n');
        lumenNodes[i] = newClientIP;
      }
    }
  }

  //Capture audio level from chip
  static uint8_t level = 0;
  thisSampleTime = millis();
  if (thisSampleTime - lastSampleTime > 5) {
    lastSampleTime = thisSampleTime;
    
    level = (uint8_t)(((int)level * 9 + ioChip.read(0, 0)) / 10);
  }

  //Send color data, rate limit to about 30FPS/PPS
  thisSendTime = millis();
  if (thisSendTime - lastSendTime > 32) {
    lastSendTime = thisSendTime;
    
    //Construct data
    
    rLumen = (uint8_t)((sin((millis() / 1000.0f) + 6.28f / 3    ) + 1.0f) * 127.0f);
    gLumen = (uint8_t)((sin((millis() / 1000.0f) + 6.28f / 3 * 2) + 1.0f) * 127.0f);
    bLumen = (uint8_t)((sin((millis() / 1000.0f) + 0            ) + 1.0f) * 127.0f);
    
    char* toSend = "s 00 00 00 00\n";
    
    byteToString(rLumen, toSend + 2);
    byteToString(gLumen, toSend + 5);
    byteToString(bLumen, toSend + 8);
    
    //byteToString(level, toSend + 11);
    
    //Send the UDP update to each discovered node
    for (int i = 0; i < MAX_LUMEN_NODES && lumenNodes[i] != 0; i++) {
      dataBroadcast.beginPacket(lumenNodes[i], 23);
      dataBroadcast.write(toSend);
      dataBroadcast.endPacket();
    }
  }
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
    clientDiscover.stop();
    
    //Wait for wifi for 5 seconds
    Serial.print("Wait\n");
    for (int i = 10; WiFi.status() != WL_CONNECTED && i > 0; i--) {
      delay(500);
    }
    if (WiFi.status() != WL_CONNECTED) {
      continue;
    }

    //Open port for lumen broadcast discovery
    clientDiscover.begin(23);

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

