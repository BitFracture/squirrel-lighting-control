/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Purpose: Adds support to spawn WiFiClientReliable classes
 * Author:  Erik W. Greif
 * Date:    2017-12-02
 */

#include "WiFiServerReliable.h"

WiFiServerReliable::WiFiServerReliable(IPAddress addr, uint16_t port) : WiFiServer(addr, port) {}

WiFiServerReliable::WiFiServerReliable(uint16_t port) : WiFiServer(port) {}

WiFiClientReliable WiFiServerReliable::available(uint8_t* status) {

	WiFiClientReliable(WiFiServer::available(status));
}
