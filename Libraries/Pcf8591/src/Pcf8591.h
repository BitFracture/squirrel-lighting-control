/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Purpose:  Enable I2C communication with Pcf8591 8-bit AD/DA chip
 * Author:   Erik W. Greif
 * Date:     2017-10-20
 */

#include <Arduino.h>
#include <Wire.h>

#pragma once

class Pcf8591 {
private:
  const uint8_t BASE_ADDRESS = 0b01001000;

  TwoWire* twoWireSource;
  bool outputEnabled;
  uint32_t timeout;
  
  uint8_t getAddress(uint8_t chipNumber);
  uint8_t getControlCode(uint8_t, uint8_t, boolean, boolean);

public:
  Pcf8591(TwoWire*);
  uint8_t read(uint8_t, uint8_t);
  uint32_t readAll(uint8_t);
  void write(uint8_t, uint8_t, bool = true);
  bool getOutputEnabled();
};

