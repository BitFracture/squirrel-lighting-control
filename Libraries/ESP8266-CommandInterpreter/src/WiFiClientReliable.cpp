/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Purpose: Wrap the awful WiFiClient (TCP socket) class to make it work better.
 *          Adds support for sequence numbers and software acks.
 * Author:  Erik W. Greif
 * Date:    2017-12-02
 */

#include "WiFiClientReliable.h"

WiFiClientReliable::WiFiClientReliable() : WiFiClient() {}

WiFiClientReliable::WiFiClientReliable(const WiFiClient& copy) : WiFiClient(copy) {}
 
size_t WiFiClientReliable::write(uint8_t u_Data) {
	
	stringData = stringData + (char)u_Data;
	
	//Send buffer to the parent write function
	if (u_Data == (uint8_t)'\n') {
		for (int character = 0; character < stringData.length(); character++) {
			WiFiClient::write((uint8_t)stringData[character]);
		}
		clear();
		sendCount++;
	}
	
	return 0x01;
}
 
void WiFiClientReliable::clear() {
	stringData = String();
}

int WiFiClientReliable::getSendCount() {
	return this->sendCount;
}