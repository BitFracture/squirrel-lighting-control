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

//Mode groups
const int MODE_COLOR  = 0; //Color mode
const int MODE_TEMP   = 1; //Temperature mode
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

//Pressure sensor
bool pressureAuto = true; //Dim lights when the user is on sensor.
uint8_t pressureDimValue = 0;

//Motion detector
unsigned long lastMotionTime = millis(); //Motion timer
int motionTimeout = 30;    //30 seconds to power off
uint8_t motionValue = 0;

bool motionEnabled  = false; //Can clap trigger power state?
bool clapEnabled  = false; //Can clap trigger power state?
bool colorAuto    = false; //Is color mode set to auto hue cycle?
bool tempAuto     = false; //Is temperature coming from light sensor?

const int CLAP_THRESHHOLD = 10; // The peak noise level of a clap.

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
  squirrelCmd.assign("get-color", onCommandGetColor);
  squirrelCmd.assign("temp", onCommandSetTemp);
  squirrelCmd.assign("get-temp", onCommandGetTemp);
  squirrelCmd.assign("brightness", onCommandSetBrightness);
  squirrelCmd.assign("get-brightness", onCommandGetBrightness);
  squirrelCmd.assign("clap", onCommandSetClap);
  squirrelCmd.assign("get-clap", onCommandGetClap);
  squirrelCmd.assign("power", onCommandSetPower);
  squirrelCmd.assign("get-power", onCommandGetPower);
  squirrelCmd.assign("motion", onCommandSetMotion);
  squirrelCmd.assign("get-motion", onCommandGetMotion);
  squirrelCmd.assign("get-mode", onCommandGetMode);
  squirrelCmd.assign("listen", onCommandSetListen);
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
      audioLevel = ioChip.read(PCF_CHIP_SELECT, PCF_PIN_AUDIO);
      avg.add(audioLevel);
      
      // Figure out if a clap has happened
      if (clapEnabled) {
        if (audioLevel <= lastLevel) {
          if (lastMaxLevel > avg.average() + CLAP_THRESHHOLD) {
            if (millis() - lastClapThreshHoldTime < 500) {
              lastClapThreshHoldTime = 0;
              OnDoubleClap();
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
    
    // Collect motion sensor data
    if (motionEnabled || outputMode == MODE_TEMP) {
      motionValue = ioChip.read(PCF_CHIP_SELECT, PCF_PIN_MOTION);
      
      if (motionEnabled) {
        if (outputMode == MODE_OFF) {
          // Motion detected
          if (motionValue > 100) {
            lastMotionTime = millis();
            setPower(POWER_ON);
          }
        } else if (millis() - lastMotionTime > motionTimeout * 1000) {
          setPower(POWER_OFF);
        } else if (motionValue > 100) {
          // Motion detected
          lastMotionTime = millis();
        }
      }
    }
    
    // Collect pressure sensor data
    if (tempAuto && clientPressure.connected()) {
      clientPressure.print("g");
      pressureLevel = (uint8_t) clientPressure.readStringUntil('\n').toInt();
    }
    
    // Collect photosensor data
    if (outputMode == MODE_TEMP && clientDaylight.connected()) {
      clientDaylight.print("g");
      brightness = (uint8_t) clientDaylight.readStringUntil('\n').toInt();
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

    //Output audio reaction
    else if (outputMode == MODE_LISTEN) {
      sprintf(toSend, "c 0 0 0 %i 0\n", 0xff & (50 * audioLevel));
    }

    //Bulb is off, output black
    else if (outputMode == MODE_OFF) {
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
 * Provides a way to turn the lumen nodes on and off while managing the previous
 * state information for you.
 */
void setPower(PowerChangeState powerState) {
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
 * Changes the saved color channels that will be sent to the bulbs when the color mode is used.
 */
void onCommandSetColor(Stream& reply, int argc, const char** argv) {  
  if (argc == 1 && strcmp(argv[0], "a") == 0)
    colorAuto = true;
  else if (argc == 3) {
    colorAuto = false;
    colorRed   = (uint8_t)atoi(argv[0]);
    colorGreen = (uint8_t)atoi(argv[1]);
    colorBlue  = (uint8_t)atoi(argv[2]);
  } else {
    reply.print("ER\n");
    return;
  }
  
  if (outputMode != MODE_OFF)
    outputMode = MODE_COLOR;
  
  reply.print("OK\n");
}

/**
 * Returns the R/G/B value that the bulbs are set to in that order.
 */
void onCommandGetColor(Stream& reply, int argc, const char** argv) {
  reply.printf("%i %i %i\n", colorRed, colorGreen, colorBlue);
}

/**
 * Changes the saved color temperature that will be sent to the bulbs when the temperature mode is used.
 */
void onCommandSetTemp(Stream& reply, int argc, const char** argv) {
  if (argc == 1) {
    if (strcmp(argv[0], "a") == 0)
      tempAuto = true;
    else {
      tempAuto = false;
      temperature = (uint8_t)atoi(argv[0]);
    }
  } else {
    reply.print("ER\n");
    return;
  }
  
  if (outputMode != MODE_OFF)
    outputMode = MODE_TEMP;
  
  reply.print("OK\n");
}

/**
 * Returns whether the temperature is off or if the temperature is on,
 * its level.
 */
void onCommandGetTemp(Stream& reply, int argc, const char** argv) {
  if (tempAuto)
    reply.printf("%i\n", temperature);
  else
    reply.println("OFF");
}

/**
 * Enables or disables automatic dimming when the pressure sensor is tripped.
 * Sets the level of dimming if the pressure sensor is tripped and enabled.
 */
void onCommandSetBrightness(Stream& reply, int argc, const char** argv) {
  if (argc == 1) {
    if (strcmp(argv[0], "auto") == 0)
      pressureAuto = true;
    else {
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
 * Returns OFF if the brightness is disabled, else it returnes the level
 * that the brightness will be dimmed.
 */
void onCommandGetBrightness(Stream& reply, int argc, const char** argv) {
  if (pressureAuto)
    reply.printf("%i\n", pressureDimValue);
  else
    reply.println("OFF");
}

/**
 * Enables or disables turning the system on and off by clapping.
 */
void onCommandSetClap(Stream& reply, int argc, const char** argv) {
  if (argc == 1 && strcmp(argv[0], "off") == 0)
    clapEnabled = false;
  else if (argc == 1 && strcmp(argv[0], "on") == 0)
    clapEnabled = true;
  else {
    reply.print("ER\n");
    return;
  }
  
  reply.print("OK\n");
}

/**
 * Returns ON or OFF depending on whether the clap system is set to
 * listen for claps.
 */
void onCommandGetClap(Stream& reply, int argc, const char** argv) {
  reply.println(clapEnabled ? "ON" : "OFF");
}

/**
 * Turns the system ON or OFF 
 */
void onCommandSetPower(Stream& reply, int argc, const char** argv) {
  if (argc == 1 && strcmp(argv[0], "off") == 0) {
    setPower(POWER_OFF);
  } else if (argc == 1 && strcmp(argv[0], "on") == 0) {
    setPower(POWER_ON);
  } else {
    reply.print("ER\n");
    return;
  }
  
  reply.print("OK\n");
}

/**
 * Returns ON or OFF depending on whether the system is currently powered
 * on off.
 */
void onCommandGetPower(Stream& reply, int argc, const char** argv) {
  reply.println(outputMode != POWER_OFF ? "ON" : "OFF");
}

/**
 * Enables or disables automatic power off when no motion is detected for
 * a set amount of time. Allows the amount of time to be set.
 */
void onCommandSetMotion(Stream& reply, int argc, const char** argv) {
  if (argc == 1) {
    if (strcmp(argv[0], "off") == 0)
      motionEnabled = false;
    else {
      motionEnabled = true;
      motionTimeout = (uint8_t)atoi(argv[0]);
      lastMotionTime = millis();
    }
  } else {
    reply.print("ER\n");
    return;
  }
  
  reply.print("OK\n");
}

/**
 * Returns the motion timeout if motion is disabled, else returns "OFF".
 */
void onCommandGetMotion(Stream& reply, int argc, const char** argv) {
  if (motionEnabled)
    reply.printf("%i\n", motionTimeout);
  else
    reply.println("OFF");
}

/**
 * Enables or disables automatic power off when no motion is detected for
 * a set amount of time. Allows the amount of time to be set.
 */
void onCommandSetListen(Stream& reply, int argc, const char** argv) {
  if (outputMode != MODE_OFF) {
    reply.print("OK\n");
    outputMode = MODE_LISTEN;
  }
  
  reply.print("ER\n");
}

/**
 * Returns the name of the mode that the system is currently in in the format
 * “MODE_[modename]”, where [modename] is all caps.
 */
void onCommandGetMode(Stream& reply, int argc, const char** argv) {
  char* modeName = "ER\n";
  switch (outputMode) {
    case MODE_COLOR: modeName = "MODE_COLOR\n"; break;
    case MODE_TEMP: modeName = "MODE_TEMP\n"; break;
    case MODE_LISTEN: modeName = "MODE_LISTEN\n"; break;
    case MODE_OFF: modeName = "MODE_OFF\n"; break;
    case MODE_YIELD: modeName = "MODE_YIELD\n"; break;
  }
  
  reply.print(modeName);
}

/**
 * Toggles turning the lumen nodes on and off.
 * This function is called when the sound sensor hears two quick claps.
 */
void OnDoubleClap() {
  setPower(POWER_TOGGLE);
}
