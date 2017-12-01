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

//Wireless nonsense
const char* WIFI_SSID = "SQUIRREL_NET";
const char* WIFI_PASS = "wj7n2-dx309-dt6qz-8t8dz";
bool reconnect = true;
long lastCheckTime = 0;

//Mode groups
const int MODE_TEMP   = 0; //Temperature mode
const int MODE_COLOR  = 1; //Color mode
const int MODE_LISTEN = 2; //Audio listen mode
const int MODE_OFF    = 3; //Lights set off
const int MODE_SLEEP  = 4; //Light off, may be woken by motion
const int MODE_YIELD  = 5; //Transmission to bulbs is disbled

int outputMode = MODE_TEMP;
int sleepingMode = MODE_TEMP;

//Channel values
uint8_t colorRed = 0;
uint8_t colorGreen = 0;
uint8_t colorBlue = 0;
uint8_t temperature = 0;
uint8_t brightness = 0;

//Mode metadata
bool clapEnabled  = false; //Can clap trigger power state?
int motionTimeout = 30;    //30 seconds to power off
bool colorAuto    = false; //Is color mode set to auto hue cycle?
bool tempAuto     = false; //Is temperature coming from light sensor?

CommandInterpreter squirrelCmd;

//Store discovered bulb IPs as they send discovery packets
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
  //squirrelCmd.assign("b", commandSetBrightness);
  //squirrelCmd.assign("cl", commandSetClap);
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

  //Limit the rate of data transmission
  thisSendTime = millis();
  if (thisSendTime - lastSendTime > 32) {
    lastSendTime = thisSendTime;
    
    //Construct command
    char toSend[32];

    //Output colors
    if (outputMode == MODE_COLOR) {

      if (colorAuto) {
        rLumen = (uint8_t)((sin((millis() / 1000.0f) + 6.28f / 3    ) + 1.0f) * 127.0f);
        gLumen = (uint8_t)((sin((millis() / 1000.0f) + 6.28f / 3 * 2) + 1.0f) * 127.0f);
        bLumen = (uint8_t)((sin((millis() / 1000.0f) + 0            ) + 1.0f) * 127.0f);
        
        sprintf(toSend, "c %i %i %i\n", rLumen, gLumen, bLumen);
      }
      else {
        sprintf(toSend, "c %i %i %i\n", colorRed, colorGreen, colorBlue);
      }
    }

    //Output audio reaction
    else if (outputMode == MODE_LISTEN) {
      
      sprintf(toSend, "c 0 0 0 %i 0\n", level);
    }

    //Output color temperature
    else if (outputMode == MODE_TEMP) {

      if (tempAuto) {
        if (clientDaylight.connected()) {
          clientDaylight.print("g\n");
          uint8_t sensorBrightness = (uint8_t)clientDaylight.readStringUntil('\n').toInt();
          sprintf(toSend, "t %i\n", sensorBrightness);
        }
      }
      else {
        sprintf(toSend, "t %i\n", temperature);
      }
    }

    //Bulb is off, output black
    else if (outputMode == MODE_OFF || outputMode == MODE_SLEEP) {
      sprintf(toSend, "c 0 0 0\n");
    }

    if (outputMode != MODE_YIELD) {
      //Send the UDP update to each discovered node
      for (int i = 0; i < MAX_LUMEN_NODES && lumenNodes[i] != 0; i++) {
        dataBroadcast.beginPacket(lumenNodes[i], 23);
        dataBroadcast.write(toSend);
        dataBroadcast.endPacket();
      }
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

  if (argv[0][0] == 'c')
    outputMode = MODE_COLOR;
  else if (argv[0][0] == 't')
    outputMode = MODE_TEMP;
  else if (argv[0][0] == 'l')
    outputMode = MODE_LISTEN;
  else if (argv[0][0] == 'o')
    outputMode = MODE_OFF;
  
  reply.print("OK\n");
}

void commandSetTemp(Stream& reply, int argc, const char** argv) {

  if (argc != 1) {
    reply.print("ER\n");
    return;
  }

  if (argv[0][0] == 'a')
    tempAuto = true;
  else {
    tempAuto = false;
    temperature = (uint8_t)atoi(argv[0]);
  }
  
  reply.print("OK\n");
}

void commandSetColor(Stream& reply, int argc, const char** argv) {
  
  if (argc == 1 && argv[0][0] == 'a') {
    colorAuto = true;
  }
  else if (argc == 3) {
    colorAuto = false;
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

