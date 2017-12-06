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

class WiFiClientReliable : public Stream {
public:
	//Invoke parent constructors
	WiFiClientReliable();
    WiFiClientReliable(WiFiClient&);
	
	virtual int read();
	virtual size_t write(uint8_t);
	
	void setTimeout(unsigned long timeout);
	void setAckTimeout(unsigned long ackTimeout);
	
	int getSendCount();
	int getReceiveCount();
	~WiFiClientReliable();
	
	//Direct function proxies
	uint8_t status() { if (client) return client->status(); else return -1; }
	virtual int connect(IPAddress ip, uint16_t port) { if (client) return client->connect(ip, port); else return -1; }
	virtual int connect(const char *host, uint16_t port) { if (client) return client->connect(host, port); else return -1; }

	virtual int available() { if (client) return client->available(); else return -1; }
	virtual int read(uint8_t *buf, size_t size) { if (client) client->read(buf, size); else return 0; }
    virtual int peek() { if (client) return client->peek(); else return -1; }
	virtual size_t peekBytes(uint8_t *buffer, size_t length) { if (client) return client->peekBytes(buffer, length); else return 0; }
	size_t peekBytes(char *buffer, size_t length) { if (client) return client->peekBytes(buffer, length); else return 0; } 
	
	virtual void flush()        { if (client) client->flush(); }
	virtual void stop()         { if (client) client->stop(); }
	virtual uint8_t connected() { if (client) return client->connected(); else return -1; }
	virtual operator bool()     { if (client) return client;              else return false; }
	
	IPAddress remoteIP()   { if (client) return client->remoteIP();   else return IPAddress(0,0,0,0); }
	uint16_t  remotePort() { if (client) return client->remotePort(); else return -1; }
	IPAddress localIP()    { if (client) return client->localIP();    else return IPAddress(0,0,0,0); }
	uint16_t  localPort()  { if (client) return client->localPort();  else return -1; }
	bool getNoDelay()      { if (client) return client->getNoDelay(); else return false; }
	void setNoDelay(bool nodelay) { if (client) return client->setNoDelay(nodelay); }
	static void setLocalPortStart(uint16_t port) { WiFiClient::setLocalPortStart(port); }
	
	static void stopAll() { WiFiClient::stopAll(); }
	static void stopAllExcept(WiFiClient * c) { WiFiClient::stopAllExcept(c); }
	
private:
	WiFiClient* client;

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