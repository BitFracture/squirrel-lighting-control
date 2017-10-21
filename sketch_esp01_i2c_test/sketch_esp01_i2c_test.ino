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
#include <Wire.h>
#include "Pcf8591.h"

Pcf8591 analogIO(&Wire);

void setup() {
  Serial.begin(9600);
  Serial.println("Initialized device");

  //Initialize I2C connection 2=data 0=clk
  Wire.begin(2, 0);
  analogIO.enableOutput(true);
}

uint8_t loopingLevel = 0;

void loop() {
  byte value0 = analogIO.read(0, 0);
  byte value1 = analogIO.read(0, 1);
  byte value2 = analogIO.read(0, 2);
  byte value3 = analogIO.read(0, 3);

  Serial.print("ADC (");
  Serial.print(value0);
  delay(5);
  Serial.print(", ");
  Serial.print(value1);
  delay(5);
  Serial.print(", ");
  Serial.print(value2);
  delay(5);
  Serial.print(", ");
  Serial.print(value3);
  delay(5);
  Serial.println(")");
  
  analogIO.write(0, loopingLevel++);

  delay(50);
}


