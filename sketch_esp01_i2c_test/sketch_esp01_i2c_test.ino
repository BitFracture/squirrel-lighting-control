/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Node:     TEST
 * Hardware: ESP8266-01[S]
 * Purpose:  Test I2C connection to AD/DA converter
 * Author:   Erik W. Greif
 * Date:     2017-10-13
 */

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

void setup() {
  Serial.begin(9600);
  Serial.println("Initialized device");

  //Initialize I2C connection 2=data 0=clk
  Wire.begin(2, 0);
}

void loop() {
  // put your main code here, to run repeatedly:

}
