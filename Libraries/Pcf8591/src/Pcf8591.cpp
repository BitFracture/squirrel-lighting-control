/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Purpose:  Enable I2C communication with Pcf8591 8-bit AD/DA chip
 * Author:   Erik W. Greif
 * Date:     2017-10-20
 */

#include "Pcf8591.h"

/**
 * Uses an existing TwoWire connection to communicate with up to 8 PCF8591 chips.
 */
Pcf8591::Pcf8591(TwoWire* twoWireSource){
  
  this->twoWireSource = twoWireSource;
  outputEnabled = false;
  timeout = 20;
}

/**
 * Reads the given analog input from the given chip.
 */
uint8_t Pcf8591::read(uint8_t chipNumber, uint8_t inputNumber) {

  uint8_t thisAddress = getAddress(chipNumber);
  
  twoWireSource->beginTransmission(thisAddress);
  twoWireSource->write(getControlCode(0, inputNumber, outputEnabled, false));
  twoWireSource->endTransmission();
 
  delayMicroseconds(60);
  twoWireSource->requestFrom(thisAddress, 2);

  //Sample two bytes, because first byte is from the previous request (see spec sheet)
  uint8_t answer = 0;
  for (int i = 0; i < 2; i++) {
    int startTime = millis();
    while (twoWireSource->available() <= 0) {
      if (startTime - millis() < timeout)
        return 0;
    }
    answer = twoWireSource->read();
  }
  return answer;
}

uint32_t Pcf8591::readAll(uint8_t chipNumber) {

	uint8_t thisAddress = getAddress(chipNumber);

	twoWireSource->beginTransmission(thisAddress);
	twoWireSource->write(getControlCode(0, 0, true, true));
	twoWireSource->endTransmission();

	delayMicroseconds(60);
	twoWireSource->requestFrom(thisAddress, 5);

	//Sample two bytes, because first byte is from the previous request (see spec sheet)
	uint32_t answer = 0;
	for (int i = 0; i < 5; i++) {
		
		int startTime = millis();
		while (twoWireSource->available() <= 0) {
			
			if (startTime - millis() < timeout)
			return 0;
		}
		uint8_t rawValue = twoWireSource->read();
		if (i == 0) continue;
		answer |= rawValue << ((i - 1) * 8);
	}
	return answer;
}

/**
 * Writes analog output to the only output pin on the given chip.
 */
void Pcf8591::write(uint8_t chipNumber, uint8_t value, bool outputEnable) {

  this->outputEnabled = outputEnable;
  uint8_t thisAddress = getAddress(chipNumber);
  
  twoWireSource->beginTransmission(thisAddress);
  twoWireSource->write(getControlCode(0, 0, outputEnabled, false));
  twoWireSource->write(value);
  twoWireSource->endTransmission();
   
  return;
}

/**
 * If disabled, output is in hi-z state even when written to (default).
 */
bool Pcf8591::getOutputEnabled() {
  return outputEnabled;
}

/**
 * Produce an address given a chip number and base address.
 */
uint8_t Pcf8591::getAddress(uint8_t chipNumber) {
  
  chipNumber = chipNumber <= 7 ? chipNumber : 7;
  return BASE_ADDRESS | chipNumber;
}

/**
 * Retrieves the control code sequence given the parameters. 
 */
uint8_t Pcf8591::getControlCode(uint8_t mode, uint8_t inputNumber, boolean outputEnable, boolean autoIncrement) {

  mode = (mode <= 3 ? mode : 3) << 4;
  inputNumber = inputNumber <= 3 ? inputNumber : 3;
  uint8_t out = ((uint8_t)outputEnable) << 6;
  uint8_t aut = ((uint8_t)autoIncrement) << 2;
  uint8_t result = (mode | inputNumber | out | aut);
  return result;
}




