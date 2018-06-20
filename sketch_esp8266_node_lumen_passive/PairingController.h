/** 
 * The Flying Squirrels: Squirrel Lighting Controller
 * Hardware: ESP8266EX
 * Purpose:  Define persistence handling
 * Author:   Erik W. Greif
 * Date:     2018-06-19
 */

#ifndef __PAIRING_CONTROLLER
#define __PAIRING_CONTROLLER

#include <ESP8266WiFi.h>
#include <EEPROM.h>

class PairingController {
private:
  ESP8266WebServer webServer(HTTP_SERVER_PORT);
  DNSServer dnsServer;

public:
  /**
   * Prepare pairing mode. 
   */
  PairingController() {
    Serial.print("Starting pairing mode\n");
  
    ledDriver.setState(true);
    ledDriver.setColor((my9291_color_t) {0, 0, 0, 0, 0});
  
    //Set up the access point
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    delay(250);
    
    WiFi.softAPConfig(pairingIp, pairingIp, IPAddress(255, 255, 255, 0));
    char* ssid = "BitLight-0000";
    memcpy(ssid + 9, persistence.getTransientId(), 4);
    if (!WiFi.softAP(ssid)) {
      Serial.printf("Access point \"%s\" could not initialize\n", ssid);
      ESP.restart();
      return;
    }
    
    dnsServer.start(DNS_PORT, "*", pairingIp);
    webServer.onNotFound(onWebRequest);
    webServer.begin();
    
    Serial.print("WiFi is ready to accept connections\n");
  }
};

#endif
