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
WiFiClient registerConnection;

const char* WIFI_SSID = "SQUIRREL_NET";
const char* WIFI_PASS = "wj7n2-dx309-dt6qz-8t8dz";
bool reconnect = true;

void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.println("Initialized");

  //Set up the wireless
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);


  
  while(WiFi.status() != WL_CONNECTED) {
    Serial.println("Waiting for connection...");
    delay(5000);
  }

  //Register this node with the controller
  Serial.print("Registering with controller");
  registerConnection.connect(IPAddress(192, 168, 1, 100), 23);
  while (!registerConnection.connected()) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nSuccess!");

  yield();
  
  String cmd = registerConnection.readStringUntil('\n');
  if (!cmd.equals("mode")) {
    Serial.print("Invalid mode command: \"");
    for (int i = 0; i < cmd.length(); i++) {
      Serial.print((int) cmd[i]);
      Serial.print(" ");
    }
    Serial.println("\"");
    registerConnection.stop();
  } else {
    registerConnection.println("persist");
    cmd = registerConnection.readStringUntil('\n');
    if (!cmd.equals("identify")) {
      Serial.print("Invalid identify command: ");
      Serial.println(cmd);
      registerConnection.stop();
    } else {
      registerConnection.println("iocontrol");
      Serial.println("\nAuthenticated!");
    }
  }
}

void loop() {
  if (iiii.connected()) {
    /*
    registerConnection.setNoDelay(false);
    setLightColor(registerConnection, 0, 0, 0, (millis() / 20) % 255);
    registerConnection.flush();
    registerConnection.setNoDelay(true);
    */
    
//    Serial.print((millis() / 200) % 255);
//    Serial.print(" \"");
//    Serial.print(registerConnection.readStringUntil('\n'));
//    Serial.println("\"");
  }
}

void handleReconnect() {
  
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
