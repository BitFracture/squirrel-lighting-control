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

//Pcf8591 ioChip(&Wire);
WiFiClient clientSquirrel;
WiFiClient clientLumen;

const char* WIFI_SSID = "SQUIRREL_NET";
const char* WIFI_PASS = "wj7n2-dx309-dt6qz-8t8dz";
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
    
void loop() {
  //Do nothing until we are connected to the server
  handleReconnect();
  
  if (clientLumen.connected()) {
    uint8_t rLumen = (uint8_t)((sin((millis() / 1000.0f) + 6.28f / 3    ) + 1.0f) * 127.0f);
    uint8_t gLumen = (uint8_t)((sin((millis() / 1000.0f) + 6.28f / 3 * 2) + 1.0f) * 127.0f);
    uint8_t bLumen = (uint8_t)((sin((millis() / 1000.0f) + 0            ) + 1.0f) * 127.0f);
    
    char* toSend = "s 00 00 00\n";
    byteToString(rLumen, toSend + 2);
    byteToString(gLumen, toSend + 5);
    byteToString(bLumen, toSend + 8);
    clientLumen.print(toSend);
    
    clientLumen.readStringUntil('\n');
  }
  else if (millis() - lastCheckTime > 1000) {
    lastCheckTime = millis();
    Serial.print("Attempt Lumen0 connect\n");
    
    //Connect to the bulb
    clientSquirrel.print("ip lumen0\n");
    String response = clientSquirrel.readStringUntil('\n');
    IPAddress lumenAddress;
    if (lumenAddress.fromString(response)) {
      if (connectClient(clientLumen, lumenAddress, 23, "iocontrol", true))
        clientLumen.setNoDelay(false);
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
    clientLumen.stop();
    
    //Wait for wifi for 5 seconds
    Serial.print("Wait\n");
    for (int i = 10; WiFi.status() != WL_CONNECTED && i > 0; i--) {
      delay(500);
    }
    if (WiFi.status() != WL_CONNECTED) {
      break;
    }

    //Try to connect persistently to squirrel
    if (connectClient(clientSquirrel, IPAddress(192, 168, 3, 1), 23, "iocontrol", true))
      reconnect = false;
  }
}

//Make static
bool connectClient(WiFiClient& server, IPAddress ip, uint16_t port, const char* identity, bool persist) {

  //Connect persistently with the controller
  Serial.print("Reg\n");
  server.connect(ip, port);

  //Wait 5 seconds for TCP connect
  for (int i = 10; !server.connected() && i > 0; i--) {
    delay(500);
  }
  if (!server.connected()) {
    server.stop();
    return false;
  }
  Serial.print("Good\n");

  server.setTimeout(5000);
  String cmd = server.readStringUntil('\n');
  if (!cmd.equals("mode")) {
    server.stop();
    return false;
  }
  else {
    if (persist)
      server.print("persist\n");
    else
      server.print("register\n");
    
    cmd = server.readStringUntil('\n');
    if (!cmd.equals("identify")) {
      server.stop();
      return false;
    }
    else {
      server.print(identity);
      server.print("\n");
    }
  }
  Serial.print("Authed\n");
  
  if (persist)
    return server.connected();
  else {
    server.stop();
    return true;
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

/*void sendBinaryColor(Stream& out, int8_t r, int8_t g, int8_t b, int8_t w) {
  
  // Create the mask by saving the first byte of rgbw in the mask
  int8_t mask = 0;
  if (w == 0)
    w = 1;
  else
    mask |= 1;
    
  mask <<= 1;

  if (b == 0)
    b = 1;
  else
    mask |= 1;
    
  mask <<= 1;

  if (g == 0)
    g = 1;
  else
    mask |= 1;
    
  mask <<= 1;

  if (r == 0)
    r = 1;
  else
    mask |= 1;

  mask |= 0x80;

  // Send the color values
  out.print("b ");
  out.write(mask);
  out.write(r);
  out.write(g);
  out.write(b);
  out.write(w);
  out.print("\n");
*/
  /*Serial.print("b ");
  Serial.print(mask);
  Serial.print(" ");
  Serial.print(r);
  Serial.print(" ");
  Serial.print(g);
  Serial.print(" ");
  Serial.print(b);
  Serial.print(" ");
  Serial.print(w);
  Serial.print("\n");*/
/*}*/
