/**
 * Creates a Stream wrapper for the WiFiUDP class. 
 */

#ifndef _UDP_STREAM_H_
#define _UDP_STREAM_H_
 
#include <WiFiUDP.h>
#include <Arduino.h>
 
class UdpStream : public Stream {
public:
	UdpStream();
	
	virtual void flush();
	virtual int read();
	virtual int peek()      { handleGetPacket(); return commandReceiving ? (int)receiveData[commandIndex] : -1; }
	virtual int available() { handleGetPacket(); return commandReceiving ? commandLength - commandIndex : 0; }
	virtual size_t write(uint8_t u_Data);
	
	virtual uint8_t begin(int);
	virtual uint8_t begin(IPAddress, int);
	virtual void stop();
	
	virtual uint8_t connected();
	
	int getSendCount() { return this->sendCounter; }
	int getReceiveCount() { return this->receiveCounter; }
	
private:
	virtual uint8_t _commonBegin();
	void handleGetPacket();
	
	char receiveBuffer[1500];
	bool commandReceiving = false;
	int commandIndex = 0;
	int commandLength = 0;
	int port;
	IPAddress address;
	String receiveData;
	String sendData;
	WiFiUDP _connection;
	uint8_t _connected = 0;
	int receiveCounter = 0;
	int sendCounter = 0;
	long sequenceNumber = 0;
	bool isServer = false;
	bool synAck = false;
};

#endif

