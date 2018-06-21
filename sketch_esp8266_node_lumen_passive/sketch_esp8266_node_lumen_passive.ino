/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Node:     Lumen (WiFi Light)
 * Hardware: ESP8266EX and MY9291/MY9231 LED Drivers
 * Purpose:  Connect to a network and receive light control data
 * Author:   Erik W. Greif
 * Date:     2018-06-19
 * 
 * ROM sizes:
 *   Thinker AI Light: 1MB, 64KB SPIFFS
 * Ideal calibrations:
 *   Thinker AI Light: 0 20 120 150 240 325
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

//--------------------------------------
//  USER-CONFIGURABLE
//--------------------------------------

//Uncomment the current hardware platform
//#define SONOFF_B1 0
#define THINKER_AILIGHT 1

//Configurable constants
const IPAddress PAIRING_IP(1, 1, 1, 1);
const uint8_t   PAIR_CYCLE_COUNT = 3;
const int       PACKET_DATA_MAX_SIZE = 64;
const uint16_t  DNS_PORT = 53;
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
    "{\"firmware\":\"%s\",\"action\":\"%s\",\"version\":%d,\"name\":\"%s\"}";

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

#elif THINKER_AILIGHT
const int LED_DATA_PIN = 13;
const int LED_CLCK_PIN = 15;
const uint8_t warmColor[] = {255, 128,  0,   64,   0};
my9291 ledDriver = my9291(LED_DATA_PIN, LED_CLCK_PIN, MY9291_COMMAND_DEFAULT);
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
  colors[3] = 128;
  colors[4] = 128;

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
    broadcast.beginPacket(broadcastAddress, persistence.getPort());
    broadcast.printf(INFO_PACKET_TEMPLATE, FIRMWARE_NAME, "discover", 
        FIRMWARE_VERSION, persistence.getName());
    broadcast.endPacket();

    lastComTime = millis();
    lastAddrCheckTime = millis();
  }
  //Keep alive packets
  if (millis() - lastKeepAliveTime > KEEP_ALIVE_PERIOD) {
    broadcast.beginPacket(broadcastAddress, persistence.getPort());
    broadcast.printf(INFO_PACKET_TEMPLATE, FIRMWARE_NAME, "keep-alive", 
        FIRMWARE_VERSION, persistence.getName());
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
  
  while (reconnect) {

    //Close the UDP data socket
    clientData.stop();
    
    //Wait for wifi for 5 seconds
    Serial.print("Wait\n");
    for (int i = 10; WiFi.status() != WL_CONNECTED && i > 0; i--) {
      delay(500);
    }
    if (WiFi.status() != WL_CONNECTED) {
      break;
    }
    Serial.print("Connected\n");

    //Open udp port for lumen data
    clientData.begin(persistence.getPort());
    
    reconnect = false;
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

void setupPair() {
  Serial.print("Booting pairing mode\n");

  ledDriver.setState(true);
  ledDriver.setColor((my9291_color_t) {0, 0, 0, 0, 0});

  //Set up the access point
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(250);
  
  WiFi.softAPConfig(PAIRING_IP, PAIRING_IP, IPAddress(255, 255, 255, 0));
  char* ssid = "Squirrel-0000";
  memcpy(ssid + 9, persistence.getTransientId(), 4);
  if (!WiFi.softAP(ssid)) {
    Serial.printf("Access point \"%s\" could not initialize\n", ssid);
    ESP.restart();
    return;
  }
  
  dnsServer.start(DNS_PORT, "*", PAIRING_IP);
  webServer.onNotFound(onWebRequest);
  webServer.begin();
  
  Serial.print("WiFi is ready to accept connections\n");
}

/**
 * The program loop while in pairing mode.
 */
void loopPair() {
  static float hue = 0;
  static uint32_t colorCycleTime = 0;
  
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

/**
 * Served all web requests, redirects all except the authentication endpoint 
 * to the authentication endpoint. This redirection (and the DNS capture) will
 * tell devices to open the captive portal for authenticating the user.
 */
void onWebRequest() {
  String uri = webServer.uri();
  if (uri.equals("/authenticate")) {
    Serial.println("Web request received");
    onAuthRequest();
  } else if (uri.equals("/reset")) {
    Serial.println("Web request received");
    onResetRequest();
  } else {
    webServer.sendHeader("Location", "http://bit.bulb/authenticate");
    webServer.send(302, "text/html", "");
  }
}

/**
 * Handles web page requests for the captive portal.
 * Refactoring should be done here.
 */
void onResetRequest() {
  if (webServer.method() == HTTP_GET) {
    webServer.send(200, "text/html", 
          "<!DOCTYPE html>\n<html>"
          "<head>"
              "<style>"
                "div.container {max-width: 400px; margin-left: auto; margin-right: auto;"
                "    background-color: #FFFFFF; padding: 24px; border-radius: 16px; }"
                "body {background-color: #555555;}"
                "input[type=text] {border: .1em solid #000000;}"
                "input[type=password] {border: .1em solid #000000;}"
                "p, label, input {font-size: 20px !important;}"
                "h1 {font-size: 48px;}"
                "h2 {font-size: 32px;}"
              "</style>"
              "<meta name=\"viewport\" content=\"width=500, target-densitydpi=device-dpi\">"
          "</head>"
          "<body>"
              "<div class=\"container\">"
                  "<h1>Squirrel</h1><h2>Factory Reset</h2>"
                  "<p>Selecting &quot;Reset Now&quot; will erase any customization to this bulb and "
                  "will establish the factory default settings. After rebooting, the "
                  "bulb will require you to enter pairing mode again to connect it to "
                  "your network.</p>"
                  "<form method=\"get\" action=\"/authenticate\">"
                      "<input type=\"submit\" value=\"Back\"></input>"
                  "</form>"
                  "<form method=\"post\">"
                      "<input type=\"submit\" value=\"Reset Now\"></input>"
                  "</form>"
              "</div>"
          "</body></html>");
  }

  //Form submission
  else if (webServer.method() == HTTP_POST) {
    
    webServer.send(200, "text/html", 
          "<!DOCTYPE html>\n<html>"
          "<head>"
              "<style>"
                "div.container {max-width: 400px; margin-left: auto; margin-right: auto;"
                "    background-color: #FFFFFF; padding: 24px; border-radius: 16px; }"
                "body {background-color: #555555;}"
                "input[type=text] {border: .1em solid #000000;}"
                "input[type=password] {border: .1em solid #000000;}"
                "p, label, input {font-size: 20px !important;}"
                "h1 {font-size: 48px;}"
                "h2 {font-size: 32px;}"
              "</style>"
              "<meta name=\"viewport\" content=\"width=500, target-densitydpi=device-dpi\">"
          "</head>"
          "<body>"
              "<div class=\"container\">"
                  "<h1>Squirrel</h1><h2>Reset complete</h2>"
                  "<p>Your Squirrel is restarting with factory settings. "
                  "If you wish to use this bulb again, enter pairing mode again and "
                  "connect the bulb to your network.</p>"
              "</div>"
          "</body></html>");

    //Set factory default settings, exit pair mode
    persistence.loadDefaults();
    persistence.dumpIfDirty();
    ESP.restart();
  }
}

void onAuthRequest() {
  if (webServer.method() == HTTP_GET) {
    webServer.send(200, "text/html", 
          "<!DOCTYPE html>\n<html>"
          "<head>"
              "<style>"
                "div.container {max-width: 400px; margin-left: auto; margin-right: auto;"
                "    background-color: #FFFFFF; padding: 24px; border-radius: 16px; }"
                "body {background-color: #555555;}"
                "input[type=text] {border: .1em solid #000000;}"
                "input[type=password] {border: .1em solid #000000;}"
                "p, label, input {font-size: 20px !important;}"
                "h1 {font-size: 48px;}"
                "h2 {font-size: 32px;}"
              "</style>"
              "<meta name=\"viewport\" content=\"width=500, target-densitydpi=device-dpi\">"
          "</head>"
          "<body>"
              "<div class=\"container\">"
                  "<h1>Squirrel</h1><h2>Connect To Your Network</h2>"
                  "<p>In order to use this Squirrel, "
                  "it must connect to your local wireless network. This is probably the "
                  "same network you use every day.<br/>Enter that information here "
                  "and submit the form.</p>"
                  "<form method=\"post\">"
                      "<label>Wi-Fi Name (SSID)</label><br/>"
                      "<input name=\"ssid\" type=\"text\" value=\"\"></input><br/>"
                      "<label>Wi-Fi Password</label><br/>"
                      "<input name=\"pass\" type=\"password\" value=\"\"></input><br/><br/>"
                      "<input type=\"submit\" value=\"Save\"></input>"
                  "</form>"
                  "<form method=\"get\" action=\"/reset\">"
                      "<input type=\"submit\" value=\"Factory Reset\"></input>"
                  "</form>"
              "</div>"
          "</body></html>");
  }

  //Form submission
  else if (webServer.method() == HTTP_POST) {
    String ssid = webServer.arg("ssid");
    String pass = webServer.arg("pass");
    bool failed = false;
    if (ssid.length() <= 1 || ssid.length() > 32)
      failed = true;
    if (pass.length() > 32)
      failed = true;

    //If failure, report it
    if (failed) webServer.send(200, "text/html", 
          "<!DOCTYPE html>\n<html>"
          "<head>"
              "<style>"
                "div.container {max-width: 400px; margin-left: auto; margin-right: auto;"
                "    background-color: #FFFFFF; padding: 24px; border-radius: 16px; }"
                "body {background-color: #555555;}"
                "input[type=text] {border: .1em solid #000000;}"
                "input[type=password] {border: .1em solid #000000;}"
                "p, label, input {font-size: 20px !important;}"
                "h1 {font-size: 48px;}"
                "h2 {font-size: 32px;}"
              "</style>"
              "<meta name=\"viewport\" content=\"width=500, target-densitydpi=device-dpi\">"
          "</head>"
          "<body>"
              "<div class=\"container\">"
                  "<h1>Squirrel</h1><h2>Something is wrong...</h2>"
                  "<p>Your network name and password may not be more than 32 characters. "
                  "Your network name may not be empty. These are the same credentials you use "
                  "to connect your phone or personal computer to your network.</p><br/>"
                  "<a href=\"/\">Try Again</a>"
              "</div>"
          "</body></html>");
      
    if (!failed) webServer.send(200, "text/html", 
          "<!DOCTYPE html>\n<html>"
          "<head>"
              "<style>"
                "div.container {max-width: 400px; margin-left: auto; margin-right: auto;"
                "    background-color: #FFFFFF; padding: 24px; border-radius: 16px; }"
                "body {background-color: #555555;}"
                "input[type=text] {border: .1em solid #000000;}"
                "input[type=password] {border: .1em solid #000000;}"
                "p, label, input {font-size: 20px !important;}"
                "h1 {font-size: 48px;}"
                "h2 {font-size: 32px;}"
              "</style>"
              "<meta name=\"viewport\" content=\"width=500, target-densitydpi=device-dpi\">"
          "</head>"
          "<body>"
              "<div class=\"container\">"
                  "<h1>Squirrel</h1><h2>Thank you</h2>"
                  "<p>Your Squirrel is restarting and will try to connect "
                  "to your network. If the bulb does not connect to your controller, try "
                  "these connection steps again, as this may be a sign that you have typed "
                  "your network settings incorrectly. If your network is an AC "
                  "(5GHz band) network, you must enable the B, G, or N wireless network "
                  "to connect this device.</p>"
              "</div>"
          "</body></html>");

    //Update network settings, exit pair mode
    if (!failed) {
      persistence.setSsid((char*)ssid.c_str());
      persistence.setPass((char*)pass.c_str());
      persistence.dumpIfDirty();
      ESP.restart();
    }
  }
}

//--------------------------------------
//  HELPER LOGIC
//--------------------------------------

/**
 * Returns the remainder of a division between numerator and denominator. 
 */
float easyFMod(float numerator, float denominator) {
  while (numerator < 0)
    numerator += denominator;
  while (numerator >= denominator)
    numerator -= denominator;

  return numerator;
}

/**
 * Locks the value of a float between two bounds.
 */
float clamp(float value, float lower, float upper) {

  if (value < lower)
    value = lower;
  if (value > upper)
    value = upper;
  return value;
}

/**
 * Note:
 * The hsvToRgb functions was copied from "https://github.com/ratkins/RGBConverter/"
 * with a free to use license.
 *
 * Converts an HSV color value to RGB. Conversion formula
 * adapted from http://en.wikipedia.org/wiki/HSV_color_space.
 * Assumes h, s, and v are contained in the set [0, 1] and
 * returns r, g, and b in the set [0, 255].
 *
 * @param   Number  h       The hue
 * @param   Number  s       The saturation
 * @param   Number  v       The value
 * @return  Array           The RGB representation
 */
void hsvToRgb(float _h, float _s, float _v, uint8_t& _r, uint8_t& _g, uint8_t& _b) {
    float h = easyFMod(_h / 360.0f, 1.0f);
    float s = clamp(_s / 100.0f, 0.0f, 1.0f);
    float v = clamp(_v / 100.0f, 0.0f, 1.0f);
    
    float r, g, b;

    int i = int(h * 6);
    float f = h * 6 - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);

    switch(i % 6){
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        case 5: r = v, g = p, b = q; break;
    }

    _r = r * 255;
    _g = g * 255;
    _b = b * 255;
}

