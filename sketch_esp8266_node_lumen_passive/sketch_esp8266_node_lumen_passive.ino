/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Node:     Lumen (WiFi Light)
 * Hardware: ESP8266EX and MY9291/MY9231 LED Drivers
 * Purpose:  Connect to a network and receive light control data
 * Author:   Erik W. Greif
 * Date:     2018-06-19
 * 
 * Hardware:
 *   AIThinker AILight: ESP8266EX + 1MB(64K SPIFFS) @80MHz
 *   Sonoff B1: ESP8285 @80MHz
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
#include <my9291.h>;
#include <DNSServer.h>
#include <ESP8266WebServer.h>

//Custom libraries
#include <CommandInterpreter.h>
#include <my9231.h>;
#include "Persistence.h"
#include "MultiStringStream.h"
#include "Helpers.h"
#include "WebStrings.h"

//--------------------------------------
//  USER-CONFIGURABLE
//--------------------------------------

//Uncomment the current hardware platform
//#define SONOFF_B1 0
#define THINKER_AILIGHT 1

//Configurable constants
const IPAddress PAIRING_IP(1, 1, 1, 1);
const uint8_t   PAIR_CYCLE_COUNT = 5;
const int       PACKET_DATA_MAX_SIZE = 64;
const uint16_t  DNS_PORT = 53;
const uint16_t  DATA_PORT = 65500;
const uint16_t  HTTP_SERVER_PORT = 80;
const uint16_t  PAIRING_RESET_TIMEOUT = 4000;
const uint16_t  CHECK_ADDRESS_TIMEOUT = 2000;
const uint16_t  POLL_CONNECTION_TIMEOUT = 5000;
const uint16_t  KEEP_ALIVE_PERIOD = 60000;
const uint16_t  FIRMWARE_VERSION = 2;
const char*     FIRMWARE_NAME = "squirrel";

//--------------------------------------
//  GLOBAL DEFINITIONS
//--------------------------------------

const char* INFO_PACKET_TEMPLATE = 
    "{\"hardware\":\"%s\",\"firmware\":\"%s\",\"action\":\"%s\","
    "\"version\":%d,\"name\":\"%s\",\"zone\":\"%s\"}";

//Defines what set of logic to use
enum BootMode {
  BOOT_PAIR = 0,
  BOOT_NORMAL = 1
};

//Per-platform definitions
#ifdef SONOFF_B1
const int LED_DATA_PIN = 12;
const int LED_CLCK_PIN = 14;
const uint8_t warmColor[] = {  0,   0,  0,    0,   255};
my9231 ledDriver = my9231(LED_DATA_PIN, LED_CLCK_PIN, MY9291_COMMAND_DEFAULT);
const char* HARDWARE_MODEL = "SONOFF_B1";

#elif THINKER_AILIGHT
const int LED_DATA_PIN = 13;
const int LED_CLCK_PIN = 15;
const uint8_t warmColor[] = {255, 128,  0,   64,   0};
my9291 ledDriver = my9291(LED_DATA_PIN, LED_CLCK_PIN, MY9291_COMMAND_DEFAULT);
const char* HARDWARE_MODEL = "AITHINKER_AILIGHT";
#endif
const uint8_t coolColor[] = {  0,   0,  0,  255,   0};

//Communication
IPAddress broadcastAddress;
WiFiUDP clientData;
WiFiUDP broadcast; 
CommandInterpreter serialCmd;
CommandInterpreter dataCmd;
ESP8266WebServer webServer(HTTP_SERVER_PORT);
DNSServer dnsServer;
bool reconnect = true;

//Misc
char packetData[PACKET_DATA_MAX_SIZE];
int bootMode = BOOT_NORMAL;
Persistence persistence;
WiFiEventHandler disconnectedEventHandler;
uint8_t colors[5];

//--------------------------------------
//  NORMAL CONTROLLER
//--------------------------------------

void setupNormal() {
  Serial.print("Booting normal mode\n");
  
  ledDriver.setState(true);
  ledDriver.setColor((my9291_color_t) {colors[0], colors[1], colors[2], colors[3], colors[4]});
  disconnectedEventHandler = WiFi.onStationModeDisconnected(&triggerReconnect);

  //Set up the wireless
  WiFi.mode(WIFI_STA);
  WiFi.begin(persistence.getSsid(), persistence.getPass());
  Serial.print("WiFi ready to connect\n");

  //Assign some commands to the command controllers
  serialCmd.assign("c", commandSetColors);
  serialCmd.assign("t", commandSetTemp);
  serialCmd.assign("hsv", commandSetColorsHsv);
  serialCmd.assign("calibrate-hue", commandSetHueCalibration);
  serialCmd.assign("set-name", commandSetName);
  serialCmd.assign("pair", commandPair);
  dataCmd = CommandInterpreter(serialCmd);
}

void triggerReconnect(const WiFiEventStationModeDisconnected& event) {
  reconnect = true;
}

/**
 * Main routine loop for typical execution.
 */
uint32_t lastComTime = 0;
void loopNormal() {
  static uint32_t cycleClearTime = millis() + PAIRING_RESET_TIMEOUT;
  static uint32_t lastAddrCheckTime = 0;
  static uint32_t lastKeepAliveTime = millis() + KEEP_ALIVE_PERIOD;
  
  //Do nothing until we are connected to the server
  handleReconnect();

  //Clear the reset counter if we pass reset cycle timeout
  if (cycleClearTime && millis() > cycleClearTime) {
    cycleClearTime = 0;
    persistence.resetCycles();
    persistence.dumpIfDirty();
  }

  //Update broadcast IP every second or so as long as we aren't actively receiving
  if (millis() - lastAddrCheckTime > CHECK_ADDRESS_TIMEOUT && millis() - lastComTime > CHECK_ADDRESS_TIMEOUT) {
    IPAddress mine = WiFi.localIP();
    IPAddress mask = WiFi.subnetMask();
    broadcastAddress = IPAddress(
        mine[0] | (~mask[0] & 255), 
        mine[1] | (~mask[1] & 255), 
        mine[2] | (~mask[2] & 255), 
        mine[3] | (~mask[3] & 255));
    Serial.printf("IP %s, subnet %s, broadcast %s\n", 
        mine.toString().c_str(), mask.toString().c_str(), 
        broadcastAddress.toString().c_str());
    lastAddrCheckTime = millis();
  }
  //If we haven't heard from anyone in a while, throw out a discovery packet
  if (millis() - lastComTime > POLL_CONNECTION_TIMEOUT) {
    
    //UDP Broadcast ourself!
    broadcast.beginPacket(broadcastAddress, DATA_PORT);
    broadcast.printf(INFO_PACKET_TEMPLATE, HARDWARE_MODEL, FIRMWARE_NAME, 
        "discover", FIRMWARE_VERSION, persistence.getName(), 
        persistence.getZone());
    broadcast.endPacket();

    lastComTime = millis();
    lastAddrCheckTime = millis();
  }
  //Keep alive packets
  if (millis() - lastKeepAliveTime > KEEP_ALIVE_PERIOD) {
    broadcast.beginPacket(broadcastAddress, DATA_PORT);
    broadcast.printf(INFO_PACKET_TEMPLATE, HARDWARE_MODEL, FIRMWARE_NAME, 
        "keep-alive", FIRMWARE_VERSION, persistence.getName(), 
        persistence.getZone());
    broadcast.endPacket();
    
    lastKeepAliveTime = millis();
  }
  
  //Handle incoming commands
  serialCmd.handle(Serial);
  dataCmd.handleUdp(clientData);
}

/**
 * Reconnection logic when a connection to the access point is lost or has
 * not been established.
 */
void handleReconnect() {
  static int state = 0;
  static long mil = 0;
  
  if (reconnect) {
    if (state == 0) {
      //Close the UDP data socket
      clientData.stop();
      
      //Wait for wifi for 5 seconds
      Serial.print("Reconnecting\n");
      state = 1;
    }
  
    //Reconnect wait loop check once per millisecond
    if (mil != millis()) {
      mil = millis();
      
      if (state > 0 && state < 5000) {
        if (WiFi.status() != WL_CONNECTED) state++;
        else {
          state = 0;
          reconnect = false;
          //Open udp port for lumen data
          clientData.begin(DATA_PORT);
          Serial.print("Connected\n");
        }
      } else{
        state = 0;
      }
    }
  } else {
    state = 0;
  }
}

/**
 * Sets the temperature of the bulb using pre-set calibration for warm and cool channels.
 * Temperature is an integer 0 to 255, where 255 is cool and 0 is warm.
 * Usage: t temp
 */
void commandSetTemp(Stream& port, int argc, const char** argv) {

  lastComTime = millis();
  
  if (argc != 1 && argc != 2) {
    return;
  }

  //The multiplier defines where we are from cool to warm
  float multiplier = (float)atoi(argv[0]) / 255.0f;
  float brightness = argc == 2 ? (float)atoi(argv[1]) / 255.0f : 1.0f;

  for (int i = 0; i < 5; i ++) {
    float channelRaw = ((float)warmColor[i] + 
                ((float)coolColor[i] - 
                (float)warmColor[i]) * multiplier);
    colors[i] = (uint8_t)(channelRaw * brightness);
  }

  ledDriver.setColor((my9291_color_t){colors[0], colors[1], colors[2], colors[3], colors[4]});
}

/**
 * Sets all of the available channels if they are provided. Requires at least RGB channels. 
 * Values are integers in the range 0 to 255.
 * Usage: c red green blue [white] [warm]
 */
void commandSetColors(Stream& port, int argc, const char** argv) {

  lastComTime = millis();

  if (argc < 3 || argc > 5) {
    return;
  }

  colors[0] = (uint8_t)atoi(argv[0]);
  colors[1] = (uint8_t)atoi(argv[1]);
  colors[2] = (uint8_t)atoi(argv[2]);
  colors[3] = argc > 3 ? (uint8_t)atoi(argv[3]) : 0;
  colors[4] = argc > 4 ? (uint8_t)atoi(argv[4]) : 0;
  
  ledDriver.setColor((my9291_color_t){colors[0], colors[1], colors[2], colors[3], colors[4]});
}

/**
 * Sets the HSV color for the bulb. This is required if you want to make use of color 
 * calibration. Pass a floating-point value [0 to 360) for hue, and [0 to 100] for value 
 * and brightness.
 * Pass a positive non-0 integer to enable calibration, or simply omit the argument.
 * Usage: hsv hue sat val [calibration-enable]
 */
void commandSetColorsHsv(Stream& port, int argc, const char** argv) {

  lastComTime = millis();

  bool calibrate = true;
  if (argc != 3 && argc != 4)
    return;
  if (argc == 4)
    calibrate = atoi(argv[3]) > 0;

  float hue = atof(argv[0]);
  if (calibrate)
    hue = calibrateHue(hue);
  hsvToRgb(hue, atof(argv[1]), atof(argv[2]),
      colors[0], colors[1], colors[2]);

  colors[3] = 0;
  colors[4] = 0;
  
  ledDriver.setColor((my9291_color_t){colors[0], colors[1], colors[2], colors[3], colors[4]});
}

/**
 * Allows the user to assign calibration boundaries for use with the hue of an HSV
 * color. For example, moving 60 to 70 would produce a yellow that was more green.
 * Usage: calibrate-hue 0 60 120 180 240 300
 */
void commandSetHueCalibration(Stream& port, int argc, const char** argv) {
  lastComTime = millis();

  if (argc != 6) {
    return;
  }

  float calibrations[] = {atof(argv[0]), atof(argv[1]), atof(argv[2]), 
      atof(argv[3]), atof(argv[4]), atof(argv[5])};

  persistence.setHues(&calibrations[0]);
  if (persistence.getIsDirty()) {
    persistence.dump();
    Serial.printf("New calibration: %d, %d, %d, %d, %d, %d\n", 
      (int)calibrations[0], (int)calibrations[1], (int)calibrations[2], 
      (int)calibrations[3], (int)calibrations[4], (int)calibrations[5]);
  }
}

/**
 * Sets the name of this bulb.
 */
void commandSetName(Stream& port, int argc, const char** argv) {

  lastComTime = millis();
  
  if (argc != 1) {
    return;
  }

  persistence.setName((char*)argv[0]);
  persistence.dumpIfDirty();
}

/**
 * Sets the name of this bulb.
 */
void commandPair(Stream& port, int argc, const char** argv) {

  lastComTime = millis();
  
  if (argc != 0) {
    return;
  }

  while (persistence.incrementAndGetCycles() < PAIR_CYCLE_COUNT);
  persistence.dumpIfDirty();
  ESP.restart();
}

/**
 * Breaks a typical color wheel into six sections, ex: red to yellow, yellow 
 * to green, green to cyan, etc. The boundaries of these regions are typically
 * separated by 60 degrees, but the RGB translations may result in inaccurate 
 * colors at the hardware level. The boundaries of these regions may be moved
 * to produce accurate colors. For example, if a bulb had a green-tinted yellow,
 * the yellow boundary could be moved from 60 down to 30 degrees (more red). 
 * 
 * The calibration boundaries are stored in ROM/Flash persistence and may be 
 * user-defined.
 */
float calibrateHue(float hue) {
  static float zoneSize = 60.0f;
  uint8_t zone = static_cast<uint8_t>(hue / zoneSize);
  uint8_t end = (zone + 1) % 6;

  const float* calibrations = persistence.getHues();
  
  //Slope doesn't work well in cycles, so bump it up one cycle
  float trueEnd = calibrations[end];
  if (end < zone)
    trueEnd += 360.0f;

  float slope = (trueEnd - calibrations[zone]) / zoneSize;
  float resultingHue = calibrations[zone] + (slope * (hue - (zone * zoneSize)));
  return easyFMod(resultingHue, 360.0f);
}

//--------------------------------------
//  PAIRING CONTROLLER
//--------------------------------------

/**
 * Sends a typical web page with custom content.
 */
void sendStandardWebPageWith200(const char* content) {
  static const void* webBuffer[10];
  webBuffer[0] = WEB_HEADER;
  webBuffer[1] = content;
  webBuffer[2] = WEB_FOOTER;
  webBuffer[3] = NULL;
  MultiStringStream resetFile(webBuffer);
  webServer.streamFile<MultiStringStream>(resetFile, "text/html");
}

/**
 * Handles web page requests for the captive portal.
 * Refactoring should be done here.
 */
void onResetRequest() {
  if (webServer.method() == HTTP_GET)
    sendStandardWebPageWith200(WEB_BODY_RESET);

  //Form submission
  else if (webServer.method() == HTTP_POST) {
    sendStandardWebPageWith200(WEB_BODY_RESET_COMPLETE);

    //Set factory default settings, exit pair mode
    persistence.loadDefaults();
    persistence.dumpIfDirty();
    ESP.restart();
  }
}

void onUpgradeCheckRequest() {
  static const char* versionUrl = "http://bitfracture.com/updates/squirrel_firmware.version";
  
  Serial.println("Checking for the latest firmware");
  Serial.printf("Firmware version URL: %s\n", versionUrl);

  HTTPClient httpClient;
  httpClient.begin(versionUrl);
  int httpCode = httpClient.GET();

  int newFirmwareVersion = -1;
  if (httpCode == HTTP_CODE_OK) {
    String responseBody = httpClient.getString();
    newFirmwareVersion = responseBody.toInt();

    Serial.printf("Current firmware version: %d\n", FIRMWARE_VERSION);
    Serial.printf("Available firmware version: %d\n", newFirmwareVersion);

    if (newFirmwareVersion > FIRMWARE_VERSION) {
      /*String fwImageURL = fwURL;
      fwImageURL.concat( ".bin" );
      t_httpUpdate_return ret = ESPhttpUpdate.update( fwImageURL );

      switch(ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          break;
      }*/
    }
  }
  else if (httpCode < 0)
    Serial.println("Could not reach firmware server");
  else
    Serial.printf("Firmware version unavailable, code %d\n", httpCode);
  httpClient.end();

  //Version strings to send to the user
  char verString[10];
  char newVerString[10];
  sprintf(&verString[0], "%d", FIRMWARE_VERSION);
  if (newFirmwareVersion > 1)
    sprintf(&newVerString[0], "%d", newFirmwareVersion);
  else
    sprintf(&newVerString[0], "ERROR");

  //Handle response concatenation and variations
  static const void* webBuffer[8];
  webBuffer[0] = WEB_HEADER;
  webBuffer[1] = WEB_BODY_FIRMWARE_CHECK_RESULTS_0;
  webBuffer[2] = &verString[0];
  webBuffer[3] = WEB_BODY_FIRMWARE_CHECK_RESULTS_1;
  webBuffer[4] = &newVerString[0];
  if (newFirmwareVersion < 0)
    webBuffer[5] = WEB_BODY_FIRMWARE_CHECK_RESULTS_2_FAIL;
  else if (newFirmwareVersion <= FIRMWARE_VERSION)
    webBuffer[5] = WEB_BODY_FIRMWARE_CHECK_RESULTS_2_GOOD;
  else
    webBuffer[5] = WEB_BODY_FIRMWARE_CHECK_RESULTS_2_READY;
  webBuffer[6] = WEB_FOOTER;
  webBuffer[7] = NULL;
  MultiStringStream responseFile(webBuffer);
  webServer.streamFile<MultiStringStream>(responseFile, "text/html");
}

void onConnectRequest() {
  if (webServer.method() == HTTP_GET)
    sendStandardWebPageWith200(WEB_BODY_CONNECT);

  //Form submission
  else if (webServer.method() == HTTP_POST) {
    String ssid = webServer.arg("ssid");
    String pass = webServer.arg("pass");
    bool failed = false;
    if (ssid.length() <= 1 || ssid.length() > 32)
      failed = true;
    if (pass.length() > 32)
      failed = true;

    //Report submission failure or success
    if (failed)
      sendStandardWebPageWith200(WEB_BODY_CONNECT_FAILED);
    else
      sendStandardWebPageWith200(WEB_BODY_CONNECT_COMPLETE);
    
    //Update network settings, exit pair mode
    if (!failed) {
      persistence.setSsid((char*)ssid.c_str());
      persistence.setPass((char*)pass.c_str());
      persistence.dumpIfDirty();
      ESP.restart();
    }
  }
}

void onHomeRequest() {
  if (webServer.method() == HTTP_GET)
    sendStandardWebPageWith200(WEB_BODY_HOME);
}

void onExitRequest() {
  if (webServer.method() == HTTP_GET)
    sendStandardWebPageWith200(WEB_BODY_EXIT);
  
  ESP.restart();
}

/**
 * Served all web requests, redirects all except the authentication endpoint 
 * to the authentication endpoint. This redirection (and the DNS capture) will
 * tell devices to open the captive portal for authenticating the user.
 */
void onWebRequest() {
  String uri = webServer.uri();
  if (uri.equals("/home")) {
    Serial.println("Web request received");
    onHomeRequest();
  } else if (uri.equals("/connect")) {
    Serial.println("Web request received");
    onConnectRequest();
  } else if (uri.equals("/reset")) {
    Serial.println("Web request received");
    onResetRequest();
  } else if (uri.equals("/exit")) {
    Serial.println("Web request received");
    onExitRequest();
  } else if (uri.equals("/firmware")) {
    Serial.println("Web request received");
    onUpgradeCheckRequest();
  } else if (uri.equals("/upgrade")) {
    Serial.println("Web request received to do nothing");
  } else {
    webServer.sendHeader("Location", "http://squirrel/home");
    webServer.send(302, "text/html", "");
  }
}

void setupPair() {
  Serial.print("Booting pairing mode\n");

  ledDriver.setState(true);
  ledDriver.setColor((my9291_color_t) {0, 0, 0, 0, 0});

  //Set up the access point
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(250);
  
  WiFi.softAPConfig(PAIRING_IP, PAIRING_IP, IPAddress(255, 255, 255, 0));
  char* ssid = "Squirrel-000000";
  const char* id = persistence.getTransientId();
  memcpy(ssid + 9, id, strlen(id));
  if (!WiFi.softAP(ssid)) {
    Serial.printf("Access point \"%s\" could not initialize\n", ssid);
    ESP.restart();
    return;
  }
  
  dnsServer.start(DNS_PORT, "*", PAIRING_IP);
  webServer.onNotFound(onWebRequest);
  webServer.begin();
  
  Serial.print("WiFi is ready to accept connections\n");

  WiFi.mode(WIFI_STA_AP);
  WiFi.begin(persistence.getSsid(), persistence.getPass());
}

/**
 * The program loop while in pairing mode.
 */
void loopPair() {
  static float hue = 0;
  static uint32_t colorCycleTime = 0;

  handleReconnect();
  
  if (millis() - colorCycleTime > 33) {
    dnsServer.processNextRequest();
    webServer.handleClient();
    
    //Color cycle when pairing
    hue += 0.005;
    if (hue >= 360.0f)
      hue -= 360.0f;
    
    hsvToRgb(calibrateHue(hue), 100.0f, 100.0f, colors[0], colors[1], colors[2]);
    colors[3] = 0;
    colors[4] = 0;
    
    ledDriver.setColor((my9291_color_t){colors[0], colors[1], colors[2], colors[3], colors[4]});
  }
}

//--------------------------------------
//  SHARED MAIN ROUTINES
//--------------------------------------

void setup() {
  WiFi.persistent(false);
  Persistence::init();
  persistence = Persistence::load();

  //Increment the cycle count and dump to flash
  uint8_t cycles = persistence.incrementAndGetCycles();
  if (cycles >= PAIR_CYCLE_COUNT) {
    bootMode = BOOT_PAIR;
    persistence.resetCycles();
  }
  persistence.dumpIfDirty();
  
  //Initial light data (remove, use persistence in a controller)
  colors[0] = 0;
  colors[1] = 0;
  colors[2] = 0;
  colors[3] = 255;
  colors[4] = 255;

  Serial.begin(9600);
  delay(250);

  switch (bootMode) {
    case BOOT_NORMAL: setupNormal(); break;
    case BOOT_PAIR: setupPair(); break;
  }
}

void loop() {
  switch (bootMode) {
    case BOOT_NORMAL: loopNormal(); break;
    case BOOT_PAIR: loopPair(); break;
  }
}

