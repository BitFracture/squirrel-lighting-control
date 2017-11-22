
#include "my9231.h"

my9231::my9231(unsigned char di, unsigned char dcki, my9291_cmd_t command, unsigned char channels) :
		implementation(di, dcki, command, channels) {
	
	pinClock = dcki;
	pinData = di;
}

void my9231::setColor(my9291_color_t color) {
	//my9291_color_t coolWarmColor = (my9291_color_t){(uint8_t)0, color.white, color.warm};
	//my9291_color_t colorColor    = (my9291_color_t){color.red,  color.green, color.blue};
    //implementation.setColor(coolWarmColor);
    //implementation.setColor(colorColor);
	
	_write(color.white, 8);
	_write(0, 8);
	_write(color.warm, 8);

	_write(0, 8);
	_write(0, 8);
	_write(0, 8);

	_write(color.green, 8);
	_write(color.red, 8);
	_write(color.blue, 8);
	delayMicroseconds(stopStartDelay);
	_pulse_di(8);
	delayMicroseconds(stopStartDelay);
}

void my9231::setColor(my9231_color_t color) {
	implementation.setColor((my9291_color_t){color.red, color.green, color.blue});
}

my9231_color_t my9231::getColor() {
	my9291_color_t rawColor = implementation.getColor();
	return (my9231_color_t){rawColor.red, rawColor.green, rawColor.blue};
}

void my9231::setState(bool state) {
	implementation.setState(state);
}

bool my9231::getState() {
	return implementation.getState();
}

/** Copied from MY9291 */
void my9231::_pulse_di(unsigned int times) {
  for (unsigned int i = 0; i < times; i++) {
    digitalWrite(pinData, HIGH);
    digitalWrite(pinData, LOW);
  }
}

/** Copied from MY9291 */
void my9231::_write(unsigned int data, unsigned char bit_length) {

    unsigned int mask = (0x01 << (bit_length - 1));

    for (unsigned int i = 0; i < bit_length / 2; i++) {
        digitalWrite(pinClock, LOW);
        digitalWrite(pinData, (data & mask));
        digitalWrite(pinClock, HIGH);
        data = data << 1;
        digitalWrite(pinData, (data & mask));
        digitalWrite(pinClock, LOW);
        digitalWrite(pinData, LOW);
        data = data << 1;
    }
}