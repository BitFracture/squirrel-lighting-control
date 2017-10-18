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
  static const int ID_MAX_COUNT = 16;      //16 clients can map to pointers
  static const int ID_WAIT_TIMEOUT = 5000; //5 seconds for auth responses
  static const int ID_MAX_REG_COUNT = 32;  //32 registered ip-name links

  char identities[ID_MAX_COUNT * (ID_LENGTH + 1)];
  WiFiClient** clients[ID_MAX_COUNT];
  int idCount = 0;
  
public: 
  TcpClientRegistrar();
  void handle(WiFiServer&);
  int assign(char*, WiFiClient**);
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

