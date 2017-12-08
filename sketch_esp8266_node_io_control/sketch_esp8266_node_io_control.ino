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
#include <UdpStream.h>
#include "AverageTracker.h"

Pcf8591 ioChip(&Wire);

UdpStream outboundClientDaylight;
UdpStream outboundClientPressure;
UdpStream inboundSquirrel;
UdpStream outboundSquirrel;

const int PORT_SQUIRREL_TO_IO = 200;
const int PORT_IO_TO_SQUIRREL = 201;
const int PORT_IO_TO_DAYLIGHT = 300;
const int PORT_IO_TO_PRESSURE = 400;

WiFiUDP clientDiscover;
WiFiUDP dataBroadcast;

////////////////////////-----------------------------------------------------------------DEBUG ONLY
TcpClientRegistrar clients;
WiFiServer listenSocket(900);
WiFiClient* clientLaptop = NULL;
////////////////////////-----------------------------------------------------------------DEBUG ONLY

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

//Current Mode (or State) of the system.
int outputMode = MODE_TEMP;

//Channel values
uint8_t colorRed = 0;
uint8_t colorGreen = 0;
uint8_t colorBlue = 0;
uint8_t temperature = 0;

//Pressure sensor
const int PRESSURE_THRESHHOLD = 200; // Will register as being pressed if sensor value (pulled high) is under this value.
static uint8_t pressureLevel = 255; // No pressure
bool brightnessAuto = true; //Dim lights when the user is on sensor.
uint8_t brightness = 0;

//Motion detector
const int MOTION_THRESHHOLD = 50; // Nomally: 0 when no motion detected, 93 when detected
unsigned long lastMotionTime = millis(); //Motion timer
int motionTimeout = 30;    //30 seconds to power off
uint8_t motionValue = 0;

//Photosensor
uint8_t photoLevel = 0;

//Audio Sensor
uint8_t audioLevel = 0;
static AverageTracker<uint8_t> avg(14);

//Mode Helpers
bool motionEnabled  = false; //Can clap trigger power state?
bool clapEnabled  = false; //Can clap trigger power state?
bool colorAuto    = false; //Is color mode set to auto hue cycle?
bool tempAuto     = true; //Is temperature coming from light sensor?

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
  inboundSquirrel.begin(PORT_SQUIRREL_TO_IO);

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
  squirrelCmd.assign("get-debug", onCommandGetDebug);
  
////////////////////////-----------------------------------------------------------------DEBUG ONLY
  clients.assign("laptop", &clientLaptop);
  listenSocket.begin();
////////////////////////-----------------------------------------------------------------DEBUG ONLY
}

/**
 * Program main loop
 */
void loop() {
  static uint8_t rLumen, gLumen, bLumen;
  static uint32_t lastSendTime, thisSendTime, thisSampleTime;
  
  //Do nothing until we are connected to the server
  handleReconnect();
  
////////////////////////-----------------------------------------------------------------DEBUG ONLY
  clients.handle(listenSocket);
////////////////////////-----------------------------------------------------------------DEBUG ONLY

  //Handle commands
  squirrelCmd.handle(Serial);
  
////////////////////////-----------------------------------------------------------------DEBUG ONLY
  if (clientLaptop && clientLaptop->connected())
    squirrelCmd.handle(*clientLaptop);
////////////////////////-----------------------------------------------------------------DEBUG ONLY
  
  squirrelCmd.handle(inboundSquirrel);
  
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
  
  if (collectSensorData(false)) {
    updateAudioMotionPowerOnOff();
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
        
        sprintf(toSend, "c %i %i %i\n", valueWithBrightness(rLumen),
                                        valueWithBrightness(gLumen),
                                        valueWithBrightness(bLumen));
      }
      else {
        sprintf(toSend, "c %i %i %i\n", valueWithBrightness(colorRed),
                                        valueWithBrightness(colorGreen), 
                                        valueWithBrightness(colorBlue));
      }
    }

    //Output color temperature
    else if (outputMode == MODE_TEMP) {
      if (tempAuto)
        sprintf(toSend, "t %i %i\n", photoLevel, valueWithBrightness(255));
      else
        sprintf(toSend, "t %i %i\n", temperature, valueWithBrightness(255));
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
  if (!outboundSquirrel.connected())
    reconnect = true;
  
  while (reconnect) {
    //Clean up all connections
    clientDiscover.stop();
    outboundClientDaylight.stop();
    outboundClientPressure.stop();
    
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
    
    WiFiClient tempClient;
    if (TcpClientRegistrar::connectClient(
	        tempClient, IPAddress(192, 168, 3, 1), 23, "iocontrol", false))
      reconnect = false;
  }
  
  static int squirrelReconnectTimeout = 0;
  if (!outboundSquirrel.connected() && millis() - squirrelReconnectTimeout > 2000) {
    squirrelReconnectTimeout = millis();
    Serial.print("Attempting connect to Squirrel\n");
    
    //Try to connect persistently to squirrel
    if (outboundSquirrel.begin(IPAddress(192, 168, 3, 1), PORT_IO_TO_SQUIRREL)) {
      Serial.print("Successfully connected to Squirrel\n");
    }
  }
  
  //Only connect to client when we are successfully connected to server
  //    delay connect attempts to 2 per second
  static uint32_t lastTimeDaylight = millis(), lastTimePressure = millis();
  connectToSlave(outboundClientDaylight, "daylight", lastTimeDaylight, PORT_IO_TO_DAYLIGHT);
  connectToSlave(outboundClientPressure, "pressure", lastTimePressure, PORT_IO_TO_PRESSURE);
}

/**
 * 
 */
void connectToSlave(UdpStream& client, const char* slaveName, uint32_t& lastTime, int port) {
  client.setTimeout(500);
  uint32_t currentTime = millis();
  if (currentTime - lastTime > 3000 && !reconnect && !client.connected()) {
    lastTime = currentTime;
    
    Serial.print("DEBUG: Attempting connect to ");
    Serial.print(slaveName);
    IPAddress ip;
    
    outboundSquirrel.printf("ip %s\n", slaveName);
    outboundSquirrel.flush();
    
    String strIp = outboundSquirrel.readStringUntil('\n');
    Serial.print(" Read Ip [ ");
    Serial.print(strIp);
    Serial.print("]");
    ip.fromString(strIp);
    Serial.print(" at IP ");
    Serial.print(ip.toString());
    
    if (ip != IPAddress(0,0,0,0) && client.begin(ip, port))
        Serial.println(" succeeded.");
    else
      Serial.println(" failed.");
  }
  client.setTimeout(1000);
}

bool collectSensorData(bool forceUpdate) {
  static uint32_t pressureLightSampleTime = millis(), adcSampleTime = millis(), currentTime;
  currentTime = millis();
  bool readData = false;
  
  if (forceUpdate || currentTime - adcSampleTime > 5) {
    // Collect data from input pins
    uint32_t pinValues = ioChip.readAll(PCF_CHIP_SELECT);
    uint8_t* pin = reinterpret_cast<uint8_t*>(&pinValues);
    audioLevel = pin[PCF_PIN_AUDIO];
    motionValue = pin[PCF_PIN_MOTION];
    
    readData = true;
    adcSampleTime = millis();
  }
  
  if (forceUpdate || currentTime - pressureLightSampleTime > 100) {
    uint8_t tempNum;
    String tempStr;
    
    // Collect pressure sensor data
    if (outboundClientPressure.connected()) {
      outboundClientPressure.print("g\n");
      outboundClientPressure.flush();
      tempStr = outboundClientPressure.readStringUntil('\n');
      if (convertNumber(tempStr.c_str(), tempNum)) {
        pressureLevel = tempNum;
      }
    }
    
    // Collect photosensor data
    if (outboundClientDaylight.connected()) {
      outboundClientDaylight.print("g\n");
      outboundClientDaylight.flush();
      tempStr = outboundClientDaylight.readStringUntil('\n');
      if (convertNumber(tempStr.c_str(), tempNum)) {
        photoLevel = tempNum;
      }
    }
    
    readData = true;
    pressureLightSampleTime = millis();
  }
  
  return readData;
}

/**
 * 
 */
void updateAudioMotionPowerOnOff() {
  static uint8_t lastAudioLevel = 0, lastMaxLevel = 0;
  static unsigned long lastClapThreshHoldTime = 0;
  
  avg.add(audioLevel);
  
  // Figure out if a clap has happened
  if (clapEnabled && outputMode != MODE_LISTEN) {
    if (audioLevel <= lastAudioLevel) {
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
  lastAudioLevel = audioLevel;
  
  // Collect motion sensor data
  if (motionEnabled) {
    if (motionValue > MOTION_THRESHHOLD) {
      // Motion detected
      if (outputMode == MODE_OFF) {
        setPower(POWER_ON);
      }
      lastMotionTime = millis();
    } else if (millis() - lastMotionTime > motionTimeout * 1000) {
      setPower(POWER_OFF);
      lastMotionTime = millis();
    }
  }
}

/**
 * Subtracts the brightness value from the inputed value if the pressure is enabled. Insures
 * that the returned value is non-negative and not wraped if a unsigned value is entered.
 */
uint8_t valueWithBrightness(uint8_t val) {
  if (brightnessAuto)
    if (pressureLevel < PRESSURE_THRESHHOLD) // Pressure sensor 
      return val / 2;
    else
      return val;
  
  return val = (val * brightness) / 255;
}

/**
 * Converts a string which contains a base 10 number to
 * to an integer of the supplied type.
 *
 * return   true of the supplied value can be held by the type {@code T} and the
 *          intirety of the supplied value is a valid integer.
 */
bool convertNumber(const char* strNum, uint8_t& numOut) {
  char* numEnd;
  long int num = strtol( strNum, &numEnd, 10 );
  numOut = static_cast<uint8_t>(num);
  return ( *strNum != 0 && num == numOut && numEnd != 0 && *numEnd == '\0');
}

/**
 * Provides a way to turn the lumen nodes on and off while managing the previous
 * state information for you.
 */
int lastOutputMode;
void setPower(PowerChangeState powerState) {
  static bool lastMotionEnabled;
  bool toggleMotion = false; // Insure motion detector doesn't wake up clapp sleep 
  
  // Turn toggle into the appropriate command
  if (powerState == POWER_TOGGLE){
    toggleMotion = true;
    powerState = (outputMode == MODE_OFF ? POWER_ON : POWER_OFF);
  }
  
  // Save state & power off or power on & restore state 
  if (outputMode != MODE_OFF && powerState == POWER_OFF) {
    lastOutputMode = outputMode;
    outputMode = MODE_OFF;
    
    if (toggleMotion)
      lastMotionEnabled = motionEnabled;
  } else if (outputMode == MODE_OFF && powerState == POWER_ON) {
    outputMode = lastOutputMode;
    
    if (toggleMotion)
      motionEnabled = lastMotionEnabled;
  }

  Serial.printf("DEBUG: Power set to %s\n", outputMode == MODE_OFF ? "OFF" : "ON");
}

/**
 * Changes the saved color channels that will be sent to the bulbs when the color mode is used.
 */
void onCommandSetColor(Stream& reply, int argc, const char** argv) { 
  bool success = false;
  uint8_t r, g, b;
  
  if (argc == 1)
    if (strcmp(argv[0], "auto") == 0) {
      colorAuto = true;
      success = true;
    } else {
      reply.print("Invalid argument value\n");
    }
  
  else if (argc == 3) {
    if (convertNumber(argv[0], r) &&
        convertNumber(argv[1], g) &&
        convertNumber(argv[2], b))
    {
      colorAuto = false;
      colorRed   = r;
      colorGreen = g;
      colorBlue  = b;
      success = true;
      
    } else {
      reply.print("Invalid argument value\n");
    }
  } else {
    reply.print("Invalid number of arguments\n");
  }
  
  if (success) {
    if (outputMode == MODE_OFF)
      lastOutputMode = MODE_COLOR;
    else
      outputMode = MODE_COLOR;
    
    reply.printf("OK\n");
  }
  reply.flush();
}

/**
 * Returns the R/G/B value that the bulbs are set to in that order.
 */
void onCommandGetColor(Stream& reply, int argc, const char** argv) {
  if (colorAuto)
    reply.printf("color auto\n");
  else
    reply.printf("%i %i %i\n", colorRed, colorGreen, colorBlue);
  
  reply.flush();
}

/**
 * Changes the saved color temperature that will be sent to the bulbs when the temperature mode is used.
 */
void onCommandSetTemp(Stream& reply, int argc, const char** argv) {
  bool success = false;
  uint8_t t;
  
  if (argc == 1) {
    if (strcmp(argv[0], "auto") == 0) {
      tempAuto = true;
      success = true;
    } else if (strcmp(argv[0], "on") == 0) {
      success = true;
    } else if (convertNumber(argv[0], t)) {
      tempAuto = false;
      temperature = t;
      success = true;
    } else {
      reply.print("Invalid argument value\n");
    }
  } else {
    reply.print("Invalid number of arguments\n");
  }
  
  if (success) {
    reply.print("OK\n");
    
    if (outputMode == MODE_OFF)
      lastOutputMode = MODE_TEMP;
    else
      outputMode = MODE_TEMP;
  }
  
  reply.flush();
}

/**
 * Returns whether the temperature is off or if the temperature is on,
 * its level.
 */
void onCommandGetTemp(Stream& reply, int argc, const char** argv) {
  if (tempAuto)
    reply.print("auto\n");
  else
    reply.printf("%i\n", temperature);
  
  reply.flush();
}

/**
 * Enables or disables automatic dimming when the pressure sensor is tripped.
 * Sets the level of dimming if the pressure sensor is tripped and enabled.
 */
void onCommandSetBrightness(Stream& reply, int argc, const char** argv) {
  bool success = false;
  uint8_t b;
  if (argc == 1) {
    if (strcmp(argv[0], "auto") == 0) {
      brightnessAuto = true;
      success = true;
    } else if (strcmp(argv[0], "on") == 0) {
      success = true;
    } else if (convertNumber(argv[0], b)) {
      brightnessAuto = false;
      brightness = b;
      success = true;
    } else {
      reply.print("Invalid argument value\n");
    }
  } else {
    reply.print("Invalid Mode\n");
  }
  
  if (success)
    reply.print("OK\n");
  
  reply.flush();
}

/**
 * Returns auto if the brightness is disabled, else it returnes the level
 * that the brightness will be dimmed.
 */
void onCommandGetBrightness(Stream& reply, int argc, const char** argv) {
  if (brightnessAuto)
    reply.print("auto\n");
  else
    reply.printf("%i\n", brightness);
  
  reply.flush();
}

/**
 * Enables or disables turning the system on and off by clapping.
 */
void onCommandSetClap(Stream& reply, int argc, const char** argv) {
  if (argc == 1 && strcmp(argv[0], "off") == 0)
    clapEnabled = false;
  else if (argc == 1 && strcmp(argv[0], "on") == 0)
    clapEnabled = true;
  else if (argc == 1) 
    reply.print("Invalid argument value\n");
  else
    reply.print("Invalid number of arguments\n");
  
  reply.printf("OK\n");
  reply.flush();
}

/**
 * Returns ON or OFF depending on whether the clap system is set to
 * listen for claps.
 */
void onCommandGetClap(Stream& reply, int argc, const char** argv) {
  reply.printf("%s\n", clapEnabled ? "ON" : "OFF");
  reply.flush();
}

/**
 * Turns the system ON or OFF 
 */
void onCommandSetPower(Stream& reply, int argc, const char** argv) {
  bool success = false;
  
  if (argc == 1) {
    if (strcmp(argv[0], "off") == 0) {
      setPower(POWER_OFF);
      success = true;
    } else if (strcmp(argv[0], "on") == 0) {
      setPower(POWER_ON);
      success = true;
    } else {
      reply.print("Invalid argument value\n");
    }
  } else {
    reply.print("Invalid number of arguments\n");
  }
  
  if (success) {
    reply.print("Success\n");
  }
  
  reply.flush();
}

/**
 * Returns ON or OFF depending on whether the system is currently powered
 * on off.
 */
void onCommandGetPower(Stream& reply, int argc, const char** argv) {
  reply.printf("%s\n", outputMode != POWER_OFF ? "ON" : "OFF");
  reply.flush();
}

/**
 * Enables or disables automatic power off when no motion is detected for
 * a set amount of time. Allows the amount of time to be set.
 */
void onCommandSetMotion(Stream& reply, int argc, const char** argv) {
  bool success = false;
  uint8_t m;
  
  if (argc == 1) {
    if (strcmp(argv[0], "off") == 0) {
      motionEnabled = false;
      success = true;
    } else if (strcmp(argv[0], "on") == 0) {
      motionEnabled = true;
      success = true;
    } else if (convertNumber(argv[0], m)) {
      motionTimeout = m;
      lastMotionTime = millis();
      motionEnabled = true;
      success = true;
    } else {
      reply.print("Invalid argument value\n");
    }
  } else {
    reply.print("Invalid number of arguments\n");
  }
  
  if (success)
    reply.printf("OK\n");
  
  reply.flush();
}

/**
 * Returns the motion timeout if motion is disabled, else returns "OFF".
 */
void onCommandGetMotion(Stream& reply, int argc, const char** argv) {
  if (motionEnabled)
    reply.printf("%i\n", motionTimeout);
  else
    reply.print("OFF\n");
  
  reply.flush();
}

/**
 * Enables or disables automatic power off when no motion is detected for
 * a set amount of time. Allows the amount of time to be set.
 */
void onCommandSetListen(Stream& reply, int argc, const char** argv) {
  if (outputMode != MODE_OFF) {
    outputMode = MODE_LISTEN;
    reply.printf("OK\n");
  } else {
    reply.print("Power is off\n");
  }
  
  reply.flush();
}

/**
 * Returns the name of the mode that the system is currently in in the format
 * “MODE_[modename]”, where [modename] is all caps.
 */
void onCommandGetMode(Stream& reply, int argc, const char** argv) {
  char* modeName = "Invalid Mode\n";
  switch (outputMode) {
    case MODE_COLOR: modeName = "MODE_COLOR\n"; break;
    case MODE_TEMP: modeName = "MODE_TEMP\n"; break;
    case MODE_LISTEN: modeName = "MODE_LISTEN\n"; break;
    case MODE_OFF: modeName = "MODE_OFF\n"; break;
    case MODE_YIELD: modeName = "MODE_YIELD\n"; break;
  }
  
  reply.print(modeName);
  reply.flush();
}

/**
 * Returns the data from all of the connected sensors in one big block.
 */
void onCommandGetDebug(Stream& reply, int argc, const char** argv) {
  collectSensorData(true);
  String strPressure(pressureLevel < PRESSURE_THRESHHOLD ? "PRESSED" : "RELEASED");
  strPressure += " {"; 
  strPressure += pressureLevel;
  strPressure += "}";
  
  String strPhotoVal;
  switch (photoLevel / 86) {
    case 0:
      strPhotoVal += "DARK";
    case 1:
      strPhotoVal += "DIM";
    default:
      strPhotoVal += "BRIGHT";
  }
  strPhotoVal += " {"; 
  strPhotoVal += photoLevel;
  strPhotoVal += "}";
  
  String strAudio;
  int audioDiff = audioLevel - avg.average();
  if (audioDiff > 1) {
    strAudio = "LOUD";
  } else if (audioDiff < -1) {
    strAudio = "QUIET";
  } else {
    strAudio = "NORMAL";
  }
  strAudio += " {"; 
  strAudio += audioLevel;
  strAudio += "}";
  
  String strMotion(motionValue > MOTION_THRESHHOLD ? "ACTIVE" : "STANDBY");
  strMotion += " {"; 
  strMotion += motionValue;
  strMotion += "}";
  
  reply.printf("Audio: %s    Motion: %s    Pressure: %s    Photocell: %s\n",
                strAudio.c_str(), strMotion.c_str(),
                (outboundClientPressure.connected() ? strPressure.c_str() : "Not connected."),
                (outboundClientDaylight.connected() ? strPhotoVal.c_str() : "Not connected."));
  
  reply.flush();
}

/**
 * Toggles turning the lumen nodes on and off.
 * This function is called when the sound sensor hears two quick claps.
 */
void OnDoubleClap() {
  setPower(POWER_TOGGLE);
}
