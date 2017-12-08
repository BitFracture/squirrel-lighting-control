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
#include <Pcf8591.h>

Pcf8591 analogIO(&Wire);

void setup() {
  delay(5000);
  
  Serial.begin(9600);
  Serial.println("Initialized device");

  //Initialize I2C connection 2=data 0=clk
  Wire.begin(2, 0);
  //analogIO.enableOutput(true);
  
  delay(5000);
}

float loopingLevel = 0.0f;
bool lastWrite = false;

uint32_t data;
uint8_t* values;

void loop() {
  
  Serial.setTimeout(50000);
  Serial.readStringUntil('\n');
    
  data = analogIO.readAll(0);
  values = (uint8_t*)&data;
  
  Serial.printf("Got values %03d %03d %03d %03d\n", values[0], values[1], values[2], values[3]);
  
  lastWrite = !lastWrite;
  analogIO.write(0, lastWrite ? 255 : 0);
}


