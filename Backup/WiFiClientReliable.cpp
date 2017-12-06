/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Purpose: Wrap the awful WiFiClient (TCP socket) class to make it work better.
 *          Adds support for sequence numbers and software acks.
 * Author:  Erik W. Greif
 * Date:    2017-12-02
 */

#include "WiFiClientReliable.h"

WiFiClientReliable::WiFiClientReliable() {
	
	client = new WiFiClient();
}

WiFiClientReliable::WiFiClientReliable(WiFiClient& toRef) {
	
	client = new WiFiClient(toRef);
}

/**
 * Reads data from the socket. When a new line is found, an ACK is sent to the 
 * remote node. 
 */
int WiFiClientReliable::read() {
	
	if (!client || !client->connected())
		return -1;
	
	int realData = client->read();
	if (realData <= 0) return realData;
	
	uint8_t data = (uint8_t)realData;
	
	//Command received, ack it
	if (data == (uint8_t)'\n') {
		
		receiveCount++;
		sendCount++;
		client->print("#ACK#\n");
	}
	
	return realData;
}

/**
 * Writes the data specified to a buffer until a new line is found.
 * Data is then sent all at once. 
 * Command blocks until the timeout waiting for an ACK packet.
 * If an ACK packet is not found within timeout, the socket is CLOSED.
 */
size_t WiFiClientReliable::write(uint8_t u_Data) {
	
	if (!client || !client->connected())
		return 1;
	
	stringData = stringData + (char)u_Data;
	
	if (u_Data == (uint8_t)'\n') {
	    //Send buffer to the parent write function
		for (int character = 0; character < stringData.length(); character++) {
			client->write((uint8_t)stringData[character]);
		}
		stringData = String();
		sendCount++;
		
		//Wait for an ACK
		client->setTimeout(ackTimeout);
		String response = client->readStringUntil('\n');
		client->setTimeout(userTimeout);
		
		if (!response.equals("#ACK#")) {
			Serial.print("[DEBUG] An ACK failed, terminating\n");
			stop();
		} else {
			receiveCount++;
		}
	}
	
	return 0x01;
}

/**
 * Gets how many messages this client has sent
 */
int WiFiClientReliable::getSendCount() {
	return this->sendCount;
}

/**
 * Gets how many messages this client has sent
 */
int WiFiClientReliable::getReceiveCount() {
	return this->receiveCount;
}

/**
 * Standard receive timeout, saved in this child class for reference.
 */
void WiFiClientReliable::setTimeout(unsigned long timeout) {
	
	userTimeout = timeout;
	
	if (client)
		client->setTimeout(timeout);
}

/**
 * How long to wait for an ack packet before terminating connection.
 */
void WiFiClientReliable::setAckTimeout(unsigned long ackTimeout) {
	
	this->ackTimeout = ackTimeout;
}

WiFiClientReliable::~WiFiClientReliable() {
	
	if (client) {
		client->stop();
		delete client;
	}
}
