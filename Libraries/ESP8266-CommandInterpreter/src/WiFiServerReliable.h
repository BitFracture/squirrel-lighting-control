/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Purpose: Adds support to spawn WiFiClientReliable classes
 * Author:  Erik W. Greif
 * Date:    2017-12-02
 */

#ifndef _WIFI_SERVER_RELIABLE_H_
#define _WIFI_SERVER_RELIABLE_H_
 
#include <WiFiServer.h>
#include "WiFiClientReliable.h"

class WiFiServerReliable : public WiFiServer {
public:
	WiFiServerReliable(IPAddress addr, uint16_t port);
    WiFiServerReliable(uint16_t port);
	
    WiFiClientReliable available(uint8_t* status = NULL);
};

#endif