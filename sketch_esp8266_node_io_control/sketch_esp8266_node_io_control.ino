/** 
 * The Flying Squirrels: Squirrel Lighting Controller
 * Node:     IO Control
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
WiFiClient clientDaylight;
WiFiUDP clientDiscover;
WiFiUDP dataBroadcast;

const char* WIFI_SSID = "SQUIRREL_NET";
const char* WIFI_PASS = "wj7n2-dx309-dt6qz-8t8dz";
bool reconnect = true;
long lastCheckTime = 0;

const int MODE_MANUAL_HUE = 0;
const int MODE_MANUAL_TEMP = 1;
const int MODE_HUE = 2;
const int MODE_AUDIO = 3;
const int MODE_TEMP = 4;

uint8_t colorRed = 0;
uint8_t colorGreen = 0;
uint8_t colorBlue = 0;
uint8_t temperature = 0;
int outputMode = MODE_MANUAL_TEMP;

CommandInterpreter squirrelCmd;

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

  squirrelCmd.assign("m", commandSetOutputMode);
  squirrelCmd.assign("c", commandSetColor);
  squirrelCmd.assign("t", commandSetTemp);
}

void loop() {
  static uint8_t rLumen, gLumen, bLumen;
  static uint32_t lastSendTime, thisSendTime, lastSampleTime, thisSampleTime;

  //Do nothing until we are connected to the server
  handleReconnect();

  //Handle commands
  squirrelCmd.handle(Serial);
  if (clientSquirrel.connected())
    squirrelCmd.handle(clientSquirrel);
  
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
    //Serial.print(",");
    //Serial.print(level);
  }

  //Send color data, rate limit to about 30FPS/PPS
  thisSendTime = millis();
  if (thisSendTime - lastSendTime > 32) {
    lastSendTime = thisSendTime;
    
    //Construct command
    char toSend[32];

    if (outputMode == MODE_HUE) {
      
      rLumen = (uint8_t)((sin((millis() / 1000.0f) + 6.28f / 3    ) + 1.0f) * 127.0f);
      gLumen = (uint8_t)((sin((millis() / 1000.0f) + 6.28f / 3 * 2) + 1.0f) * 127.0f);
      bLumen = (uint8_t)((sin((millis() / 1000.0f) + 0            ) + 1.0f) * 127.0f);
      
      sprintf(toSend, "c %i %i %i\n", rLumen, gLumen, bLumen);
    }
    else if (outputMode == MODE_AUDIO) {
      
      sprintf(toSend, "c 0 0 0 %i 0\n", level);
    }
    else if (outputMode == MODE_MANUAL_HUE) {
      
      sprintf(toSend, "c %i %i %i\n", colorRed, colorGreen, colorBlue);
    }
    else if (outputMode == MODE_MANUAL_TEMP) {
      
      sprintf(toSend, "t %i\n", temperature);
    }
    else if (outputMode == MODE_TEMP) {
      
      if (clientDaylight.connected()) {
        clientDaylight.print("g\n");
        uint8_t sensorBrightness = (uint8_t)clientDaylight.readStringUntil('\n').toInt();
        sprintf(toSend, "t %i\n", sensorBrightness);
      }
    }
    
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

  //Reconnect if server TCP connection lost
  if (!clientSquirrel.connected())
    reconnect = true;
  
  while (reconnect) {

    //Clean up all connections
    clientSquirrel.stop();
    clientDiscover.stop();
    clientDaylight.stop();
    
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

  //Only connect to clientDaylight when we are successfull connected to server
  //    delay connect attempts to 2 per second
  static uint32_t lastTime = millis();
  uint32_t currentTime = millis();
  if (currentTime - lastTime > 500 && !reconnect && !clientDaylight.connected()) {
    Serial.print("Attempt connect to daylight\n");
    lastTime = currentTime;
    
    clientSquirrel.print("ip daylight\n");
    IPAddress daylightIp;
    if (daylightIp.fromString(clientSquirrel.readStringUntil('\n')) && daylightIp != 0) {
      
      //Persistent connect to daylight
      TcpClientRegistrar::connectClient(clientDaylight, daylightIp, 23, "iocontrol", true);
    }
  }
}

void commandSetOutputMode(Stream& reply, int argc, const char** argv) {

  if (argc != 1) {
    reply.print("ER\n");
    return;
  }

  if (argv[0][0] == '0')
    outputMode = MODE_MANUAL_HUE;
  else if (argv[0][0] == '1')
    outputMode = MODE_MANUAL_TEMP;
  else if (argv[0][0] == '2')
    outputMode = MODE_HUE;
  else if (argv[0][0] == '3')
    outputMode = MODE_AUDIO;
  else if (argv[0][0] == '4')
    outputMode = MODE_TEMP;
  
  reply.print("OK\n");
}

void commandSetTemp(Stream& reply, int argc, const char** argv) {

  if (argc != 1) {
    reply.print("ER\n");
    return;
  }

  if (argv[0][0] == 'a')
    outputMode = MODE_TEMP;
  else {
    outputMode = MODE_MANUAL_TEMP;
    temperature = (uint8_t)atoi(argv[0]);
  }
  
  reply.print("OK\n");
}

void commandSetColor(Stream& reply, int argc, const char** argv) {

  if (argc == 1 && argv[0][0] == 'a') {
    outputMode = MODE_HUE;
  }
  else if (argc == 3) {
    outputMode = MODE_MANUAL_HUE;
    colorRed   = (uint8_t)atoi(argv[0]);
    colorGreen = (uint8_t)atoi(argv[1]);
    colorBlue  = (uint8_t)atoi(argv[2]);
  }
  else {
    reply.print("ER\n");
    return;
  }
  
  reply.print("OK\n");
}

