/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Purpose: Wrap the awful WiFiClient (TCP socket) class to make it work better.
 *          Adds support for sequence numbers and software acks.
 * Author:  Erik W. Greif
 * Date:    2017-12-02
 */

#ifndef _WIFI_CLIENT_RELIABLE_H_
#define _WIFI_CLIENT_RELIABLE_H_
 
#include <WiFiClient.h>

class WiFiClientReliable : public WiFiClient {
public:
	//Invoke parent constructors
	WiFiClientReliable();
    WiFiClientReliable(const WiFiClient&);
	
	virtual int read();
	virtual size_t write(uint8_t);
	void setTimeout(unsigned long timeout);
	void setAckTimeout(unsigned long ackTimeout);
	
	int getSendCount();
	int getReceiveCount();

private:
	String stringData;
	Stream* sourceStream = NULL;
	int sendCount = 0;
	int receiveCount = 0;
	bool _ackEnable = true; //NOT user configurable
	bool _ackWaitEnable = true; //NOT user configurable
	int ackTimeout = 5000;
	int userTimeout = 1000;
};

#endif