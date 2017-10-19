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

const uint8_t PCF8591 = 0x90 >> 1;

void setup() {
  Serial.begin(9600);
  Serial.println("Initialized device");

  //Initialize I2C connection 2=data 0=clk
  Wire.begin(2, 0);
}

void loop() {
  // put your main code here, to run repeatedly:
  Wire.beginTransmission(PCF8591); //Activate
  Wire.write(0b00000001);          //Control: read starting at A0
  Wire.endTransmission();
  Wire.requestFrom(PCF8591, 1);
  byte value0 = Wire.read();

  Serial.print("ADC: ");
  Serial.println(value0);

  delay(1000);
}


