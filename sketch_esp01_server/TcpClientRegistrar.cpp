/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Purpose: Automatically attach TCP clients to streams based on an identifier
 * Author:  Erik W. Greif
 * Date:    2017-10-14
 */

#include "TcpClientRegistrar.h"

TcpClientRegistrar::TcpClientRegistrar() {
}

int TcpClientRegistrar::assign(char* identity, WiFiClient** clientPointerAddress) {
  if (idCount >= ID_MAX_COUNT)
    return -1;

  clients[idCount] = clientPointerAddress;
  int nameOffset = (ID_LENGTH + 1) * idCount;
  for (int i = 0; identity[i] != '\0' && i < ID_LENGTH; i++) {;

    identities[i + nameOffset] = identity[i];
    identities[i + nameOffset + 1] = '\0';
  }
  
  return idCount++;
}

/**
 * Handle any incoming client connections on the given server, and automatically
 * determine the identity and address the client to the right pointer. 
 * An existing connection with the same identity will be closed. 
 */
void TcpClientRegistrar::handle(WiFiServer& listenServer) {
  
  WiFiClient newClient = listenServer.available();
  if (!newClient)
    return;
  
  while (!newClient.connected())
    delay(1);

  //Wait for the new client to respond to identify command
  newClient.setTimeout(ID_WAIT_TIMEOUT);
  newClient.println("identify");
  String clientId = newClient.readStringUntil('\n');
  clientId.replace("\r", "");

  Serial.print("DEBUG: Client incoming with identity=\"");
  Serial.print(clientId);
  Serial.println("\"");

  //Find which identity was provided
  for (int i = 0; i < idCount; i++) {
    const char* name = (const char*)&identities[i * (ID_LENGTH + 1)];
    WiFiClient** clientAdd = clients[i];
    
    if (clientId.equals(name)) {
      if (*clientAdd) {
        Serial.print("DEBUG: Closing previous connection for ");
        Serial.println(name);
        (*clientAdd)->stop();
        delete (*clientAdd);
      }
      (*clientAdd) = new WiFiClient(newClient);
      break;
    }
  }
  newClient.stop();
}
