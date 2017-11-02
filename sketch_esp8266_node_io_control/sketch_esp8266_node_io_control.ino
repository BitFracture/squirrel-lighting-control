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

  disconnectedEventHandler = WiFi.onStationModeDisconnected(&triggerReconnect);

  //Set up the wireless
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void loop() {
  //Do nothing until we are connected to the server
  handleReconnect();
  
  if (clientLumen.connected()) {
    /*
    clientSquirrel.setNoDelay(false);
    setLightColor(clientSquirrel, 0, 0, 0, (millis() / 20) % 255);
    clientSquirrel.flush();
    clientSquirrel.setNoDelay(true);
    */

    /*
    Serial.print((millis() / 200) % 255);
    Serial.print(" \"");
    Serial.print(clientSquirrel.readStringUntil('\n'));
    Serial.print("\"\n");
    */
  }
  else if (millis() - lastCheckTime > 1000) {
    lastCheckTime = millis();
    
    //Connect to the bulb
    clientSquirrel.print("ip lumen0\n");
    String response = clientSquirrel.readStringUntil("\n");
    IPAddress lumenAddress;
    if (lumenAddress.fromString(response)) {
      //TODO: Use a shared connection function, because tired of writing this crap
    }
  }
}

void triggerReconnect(const WiFiEventStationModeDisconnected& event) {

  reconnect = true;
}

void handleReconnect() {

  //TODO: Reconnect if server TCP connection lost
  
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
  
    //Connect persistently with the controller
    Serial.print("Reg\n");
    clientSquirrel.connect(IPAddress(192, 168, 3, 1), 23);

    //Wait 5 seconds for TCP connect
    for (int i = 10; !clientSquirrel.connected() && i > 0; i--) {
      delay(500);
    }
    if (!clientSquirrel.connected()) {
      clientSquirrel.stop();
      break;
    }
    Serial.print("Good\n");

    clientSquirrel.setTimeout(5000);
    String cmd = clientSquirrel.readStringUntil('\n');
    if (!cmd.equals("mode")) {
      clientSquirrel.stop();
      break;
    }
    else {
      clientSquirrel.print("persist\n");
      cmd = clientSquirrel.readStringUntil('\n');
      if (!cmd.equals("identify")) {
        clientSquirrel.stop();
        break;
      }
      else {
        clientSquirrel.print("iocontrol\n");
      }
    }
    Serial.print("Authed\n");

    if (clientSquirrel.connected())
      reconnect = false;
  }
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
