/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Purpose: Automatically attach TCP clients to streams based on an identifier
 * Author:  Erik W. Greif
 * Date:    2017-10-14
 */

#pragma once

#include <ESP8266HTTPClient.h>
#include <WiFiServer.h>
#include <WiFiClient.h>

//Directly link to ESP SDK
extern "C" {
#include "user_interface.h"
}

class TcpClientRegistrar {
private:
  static const int ID_LENGTH = 16;         //Identities < 16 chars
  static const int ID_MAX_COUNT = 8;       //8 clients can map to pointers (5 is max TCP on 8266)
  
  static const int ID_MAX_REG_COUNT = 64;  //64 registered ip-name links
  char registrar[ID_MAX_REG_COUNT * (ID_LENGTH + 1)];
  uint32_t registrarIps[ID_MAX_REG_COUNT];

  uint16_t flushDelay;

  char identities[ID_MAX_COUNT * (ID_LENGTH + 1)];
  WiFiClient** clients[ID_MAX_COUNT];
  int idCount = 0;
  int idWaitCount = 100;

  void setIp(const char*, IPAddress);
  
public: 
  TcpClientRegistrar();
  void handle(WiFiServer&);
  int assign(char*, WiFiClient**);
  IPAddress findIp(const char*);
  void enableInitialFlush(int = 20);
  void disableInitialFlush();
  void setConnectionTimeout(int);
  int getConnectionTimeout();
  
  static bool connectClient(
      WiFiClient&, IPAddress, uint16_t, const char*, bool = true);
};

/**
 * Add wrapper function to change max AP connection quantity.
 */
inline boolean softAPSetMaxConnections(int quantity) {
  struct softap_config config;
  wifi_softap_get_config(&config);
  config.max_connection = quantity;
  return wifi_softap_set_config(&config);
}

