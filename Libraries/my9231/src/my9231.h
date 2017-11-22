/*

Wrapper for the MY9291 class that works with MY9231 chips
Provides support for sending MY9291 4 or 5-channel color to two MY9231 chips. 

*/

#ifndef _my9291_inc_h
#define _my9291_inc_h

#include <my9291.h>

#endif

#ifndef _my9231_h
#define _my9231_h

typedef struct {
    unsigned int red;
    unsigned int green;
    unsigned int blue;
} my9231_color_t;

class my9231 {
public:
	my9231(unsigned char di, unsigned char dcki, my9291_cmd_t command, unsigned char channels = 5);
	void setColor(my9291_color_t color);
	void setColor(my9231_color_t color);
	my9231_color_t getColor();
	void setState(bool state);
	bool getState();

private:
	void _pulse_di(unsigned int times);
	void _write(unsigned int data, unsigned char bit_length);

	my9291 implementation;
	int pinClock;
	int pinData;
	int stopStartDelay = 200; //Microseconds
};

#endif
