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
#include <ESP8266Ping.h>

#include <TcpClientRegistrar.h>
#include <CommandInterpreter.h>
#include <Pcf8591.h>
#include "AverageTracker.h"

Pcf8591 ioChip(&Wire);
WiFiClient clientSquirrel;
WiFiClient clientDaylight;
WiFiClient clientPressure;
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
const int MODE_YIELD  = 4; //Transmission to bulbs is disbled

//IO chip definitions
const int PCF_PIN_AUDIO = 0;
const int PCF_PIN_MOTION = 1;
const int PCF_CHIP_SELECT = 0;

int outputMode = MODE_TEMP;

//Channel values
uint8_t colorRed = 0;
uint8_t colorGreen = 0;
uint8_t colorBlue = 0;
uint8_t temperature = 0;
uint8_t brightness = 0;

//Mode metadata
bool pressureAuto = true; //Dim lights when the user is on sensor.
uint8_t pressureDimValue = 0;

int motionTimeout = 30;    //30 seconds to power off
bool motionEnabled  = false; //Can clap trigger power state?
bool clapEnabled  = false; //Can clap trigger power state?
bool colorAuto    = false; //Is color mode set to auto hue cycle?
bool tempAuto     = false; //Is temperature coming from light sensor?

const int CLAP_THRESHHOLD = 10;

//States which the on/off state can be asked to changed to.
enum PowerChangeState {POWER_ON, POWER_OFF, POWER_TOGGLE};

CommandInterpreter squirrelCmd;

//Store discovered bulb IPs as they send discovery packets
const int MAX_LUMEN_NODES = 16;
IPAddress lumenNodes[MAX_LUMEN_NODES];

WiFiEventHandler disconnectedEventHandler;

/**
 * Configured wifi, sets up command interpreters.
 */
void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.print("Initialized\n");
  Wire.begin(2, 0);

  //Set up the wireless
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  disconnectedEventHandler = WiFi.onStationModeDisconnected(&triggerReconnect);

  squirrelCmd.assign("color", onCommandSetColor);
  squirrelCmd.assign("temp", onCommandSetTemp);
  squirrelCmd.assign("brightness", onCommandSetBrightness);
  // squirrelCmd.assign("clap", onCommandSetTemp);
  // squirrelCmd.assign("power", onCommandSetTemp);
  // squirrelCmd.assign("listen", onCommandSetTemp);
}


/**
 * Program main loop
 */
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

  // Collect data from inputs
  
  //    Audio Data Variables
  static uint8_t audioLevel = 0, lastLevel = 0, lastMaxLevel = 0;
  static unsigned long lastClapThreshHoldTime = 0;
  static AverageTracker<uint8_t> avg(14);
  
  //    Pressure sensor values
  static uint8_t pressureLevel = 255; // No pressure
  
  thisSampleTime = millis();
  if (thisSampleTime - lastSampleTime > 5) {
    
    // Capture audio level from chip
    if (MODE_LISTEN || clapEnabled) {
      // Record current audio value
      audioLevel = ioChip.read(0, 0);
      avg.add(audioLevel);
      
      // Figure out if a clap has happened
      if (clapEnabled) {
        if (audioLevel <= lastLevel) {
          if (lastMaxLevel > avg.average() + CLAP_THRESHHOLD) {
            if (millis() - lastClapThreshHoldTime < 500) {
              OnDoubleClap();
              lastClapThreshHoldTime = 0;
            } else {
              lastClapThreshHoldTime = millis();
            }
            lastMaxLevel = -1;
          }
          lastMaxLevel = 0;
        } else {
          lastMaxLevel = audioLevel;
        }
      }
      
      // Update old audio value
      lastLevel = audioLevel;
    }
    
    // Collect pressure sensor data
    if (clientDaylight.connected()) {
      clientDaylight.print("g");
      pressureLevel = (uint8_t) clientDaylight.readStringUntil('\n').toInt();
    }
  }

  // Changes outputs in a given time span
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
      sprintf(toSend, "c 0 0 0 %i 0\n", 0xff & (50 * audioLevel));
    }

    //Output color temperature
    else if (outputMode == MODE_TEMP) {
      if (tempAuto) {
        if (clientDaylight.connected()) {
          clientDaylight.print("g\n");
          String response = clientDaylight.readStringUntil('\n');
          if (response.length() > 0)
            temperature = (uint8_t)response.toInt();
          else
            clientDaylight.stop();
        }
      }
      
      sprintf(toSend, "t %i\n", temperature);
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

  handleHeartbeat();
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

/**
 * Interrupt called when wireless drops.
 */
void triggerReconnect(const WiFiEventStationModeDisconnected& event) {
  reconnect = true;
}


/**
 * Takes care of tearing down any open connections when wireless is lost. Then the process is as follows:
 * 1. Connect to wifi
 * 2. Register with server
 * 3. Open any other dependent connections
 */
void handleReconnect() {

  //Reconnect if server TCP connection lost
  if (!clientSquirrel.connected())
    reconnect = true;
  
  while (reconnect) {

    //Clean up all connections
    clientSquirrel.stop();
    clientDiscover.stop();
    clientDaylight.stop();
    clientPressure.stop();
    
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

  //Only connect to clientDaylight when we are successfully connected to server
  //    delay connect attempts to 2 per second
  static uint32_t lastTime = millis();
  uint32_t currentTime = millis();
  if (currentTime - lastTime > 3000 && !reconnect && !clientDaylight.connected()) {
    Serial.print("DEBUG: Attempting connect to daylight...");
    lastTime = currentTime;
    
    clientSquirrel.print("ip daylight\n");
    IPAddress daylightIp;
    if (daylightIp.fromString(clientSquirrel.readStringUntil('\n')) && daylightIp != 0) {
      Serial.print(" got IP...");
      
      //Persistent connect to daylight if we can ping
      if (Ping.ping(daylightIp, 1)) {
        Serial.print(" ping success\n");
        TcpClientRegistrar::connectClient(clientDaylight, daylightIp, 23, "iocontrol", true);
      } else {
        Serial.print(" ping failed\n");
      }
    } else {
      
      Serial.print(" IP not found\n");
    }
  }
}

/**
 * Changes the saved color temperature that will be sent to the bulbs when the temperature mode is used.
 */
void onCommandSetTemp(Stream& reply, int argc, const char** argv) {
  if (outputMode != MODE_OFF) {
    if (argc != 1) {
      reply.print("ER\n");
      return;
    }

    if (strcmp(argv[0], "a") == 0)
      tempAuto = true;
    else {
      tempAuto = false;
      temperature = (uint8_t)atoi(argv[0]);
    }
  }
  reply.print("OK\n");
}

/**
 * Changes the saved color channels that will be sent to the bulbs when the color mode is used.
 */
void onCommandSetColor(Stream& reply, int argc, const char** argv) {
  if (outputMode != MODE_OFF)
    outputMode = MODE_COLOR;
  
  if (argc == 1 && strcmp(argv[0], "a") == 0)
    colorAuto = true;
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

/**
 * Changes the actions of the pressure sensor.
 */
void onCommandSetBrightness(Stream& reply, int argc, const char** argv) {
  if (argc == 1) {
    if (strcmp(argv[0], "auto")) {
      pressureAuto = true;
    } else {
      pressureAuto = false;
      pressureDimValue = (uint8_t)atoi(argv[0]);
    }
  } else {
    reply.print("ER\n");
    return;
  }
  
  reply.print("OK\n");
}

/**
 * Provides a way to turn the lumen nodes on and off while managing the previous
 * state information for you.
 */
void setpower(PowerChangeState powerState) {
  static int lastOutputMode;
  
  // Turn toggle into the appropriate command
  if (powerState == POWER_TOGGLE)
    powerState = (outputMode == MODE_OFF ? POWER_ON : POWER_OFF);
  
  // Save state & power off or power on & restore state 
  if (outputMode != MODE_OFF && powerState == POWER_OFF) {
    lastOutputMode = outputMode;
  } else if (outputMode == MODE_OFF && powerState == POWER_ON) {
    outputMode = lastOutputMode;
  }
}

/**
 * Toggles turning the lumen nodes on and off.
 * This function is called when the sound sensor hears two quick claps.
 */
void OnDoubleClap() {
  setpower(POWER_TOGGLE);
}
