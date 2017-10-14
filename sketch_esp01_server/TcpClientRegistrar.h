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

class TcpClientRegistrar {
private:
  static const int ID_LENGTH = 16;
  static const int ID_MAX_COUNT = 16;
  static const int ID_WAIT_TIMEOUT = 5000;

  char identities[ID_MAX_COUNT * (ID_LENGTH + 1)];
  WiFiClient** clients[ID_MAX_COUNT];
  int idCount = 0;
  
public: 
  TcpClientRegistrar();
  void handle(WiFiServer&);
  int assign(char*, WiFiClient**);
};

