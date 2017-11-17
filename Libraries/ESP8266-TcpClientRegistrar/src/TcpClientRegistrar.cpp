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
  for (int i = 0; identity[i] != '\0' && i < ID_LENGTH; i++) {

    identities[i + nameOffset] = identity[i];
    identities[i + nameOffset + 1] = '\0';
  }
  
  return idCount++;
}

/**
 * Enables a wait period to capture and discard any initial incoming data
 * when a TCP client connects. This is important when using keyboard input
 * through a TelNet client, because some clients send identifying 
 * information when they connect. This information must not interfere
 * with the 'identify' and 'mode' queries. 
 * 
 * @param timeout  The number of milliseconds to wait for data to be present. Default=20.
 */
void TcpClientRegistrar::enableInitialFlush(int timeout) {
  flushDelay = timeout > 0 ? timeout : 0;
}

/**
 * Disable flushing initial data from the connection. Default behavior. 
 */
void TcpClientRegistrar::disableInitialFlush() {
  flushDelay = 0;
}

void TcpClientRegistrar::setConnectionTimeout(int timeout) {
  idWaitCount = timeout < 0 ? 0 : timeout;
}

int TcpClientRegistrar::getConnectionTimeout() {
  return idWaitCount;
}

IPAddress TcpClientRegistrar::findIp(const char* identity) {

  for (int index = 0; index < ID_MAX_REG_COUNT; index++) {
    const char* viewing = (const char*)&registrar[index * (ID_LENGTH + 1)];
    if (strcmp(viewing, identity) == 0) {
      return IPAddress(registrarIps[index]);
    }
  }
  
  return IPAddress(0, 0, 0, 0);
}

/**
 * Private
 * Registers an identity with an IP address.
 * 
 * TO-DO: Set priority indexes based on past retrievals. 
 *        Overwrite rather than filling up. Security flaw!
 */
void TcpClientRegistrar::setIp(const char* identity, IPAddress ip) {

  int index = 0;
  for (; index < ID_MAX_REG_COUNT; index++) {
    const char* viewing = (const char*)&registrar[index * (ID_LENGTH + 1)];
    if (viewing[0] == '\0' || strcmp(viewing, identity) == 0)
      break;
  }

  //NO MORE ADDRESSES
  if (index >= ID_MAX_REG_COUNT)
    return;

  //Copy identity (first chars)
  for (int i = 0; i < ID_LENGTH; i++) {
    registrar[(index * (ID_LENGTH + 1)) + i] = identity[i];
    registrar[(index * (ID_LENGTH + 1)) + i + 1] = '\0';
  }
  
  registrarIps[index] = uint32_t(ip);
}

/**
 * Handle any incoming client connections on the given server, and automatically
 * determine the identity and address the client to the right pointer. 
 * An existing connection with the same identity will be closed. 
 */
void TcpClientRegistrar::handle(WiFiServer& listenServer) {

  //Check old clients, clear up disconnects
  for (int i = 0; i < idCount; i++) {
    if ((*clients[i]) != NULL && !(*clients[i])->connected()) {
      (*clients[i])->stop();
      *clients[i] = NULL;
      Serial.print("DEBUG: Cleaned up connection ");
      Serial.print((const char*)&identities[i * (ID_LENGTH + 1)]);
	  Serial.print("\n");
    }
  }

  //Find new clients
  WiFiClient newClient = listenServer.available();
  if (!newClient)
    return;

  //TO-DO Allow setting this timeout (default to 20mS)
  //Wait for the new connection to open, fail after timeout
  newClient.setNoDelay(true);
  for (int i = 0; !newClient.connected() && i < 250; i++)
    delay(1);
  if (!newClient.connected()) {
    newClient.stop();
    return;
  }

  //Catch and discard any initial crap
  if (flushDelay > 0) {
    delay(flushDelay);
    while (newClient.available()) {
      newClient.flush();
      delay(flushDelay);
    }
  }

  //Get the mode from the client
  newClient.setTimeout(idWaitCount);
  newClient.print("mode\n");
  String clientMode = newClient.readStringUntil('\n');
  clientMode.replace("\r", "");
  newClient.setTimeout(0);

  int clientModeVal = 0;
  if (clientMode.equals("persist"))
    clientModeVal = 0;
  else if (clientMode.equals("register"))
    clientModeVal = 1;
  else {
    newClient.stop();
    return;
  }

  //Get the identity from the client
  newClient.setTimeout(idWaitCount);
  newClient.print("identify\n");
  String clientId = newClient.readStringUntil('\n');
  clientId.replace("\r", "");
  newClient.setTimeout(0);

  if (clientModeVal == 0)
    Serial.print("DEBUG: Persistent client has identity=\"");
  else
    Serial.print("DEBUG: Registering client has identity=\"");
  Serial.print(clientId);
  Serial.print("\"\n");

  //Register this client in the lookup table
  setIp(clientId.c_str(), newClient.remoteIP());

  if (clientModeVal == 1) {
    newClient.stop();
    return;
  }
  
  //Find which identity was provided
  for (int i = 0; i < idCount; i++) {
    const char* name = (const char*)&identities[i * (ID_LENGTH + 1)];
    WiFiClient** clientAdd = clients[i];
    
    if (clientId.equals(name)) {
      if (*clientAdd) {
        Serial.print("DEBUG: Closing previous connection for ");
        Serial.print(name);
        Serial.print("\n");
        (*clientAdd)->stop();
        delete (*clientAdd);
      }
      (*clientAdd) = new WiFiClient(newClient);
      break;
    }
  }
  newClient.stop();
}

bool TcpClientRegistrar::connectClient(WiFiClient& server, IPAddress ip, uint16_t port, const char* identity, bool persist) {

  Serial.print("TCP Connect as ");
  Serial.print(identity);
  Serial.print(" to ");
  Serial.print(ip);
  Serial.print(":");
  Serial.print(port);
  Serial.print("\n");
  
  //Connect now
  server.connect(ip, port);

  //Wait 1 second for TCP connect
  for (int i = 10; !server.connected() && i > 0; i--) {
    delay(100);
  }
  if (!server.connected()) {
    Serial.print("TIMEOUT\n");
    server.stop();
    return false;
  }
  Serial.print("Connected\n");

  server.setTimeout(1000);
  String cmd = server.readStringUntil('\n');
  if (!cmd.equals("mode")) {
    Serial.print("BAD REQ\n");
    server.stop();
    return false;
  }
  else {
    if (persist)
      server.print("persist\n");
    else
      server.print("register\n");
    
    cmd = server.readStringUntil('\n');
    if (!cmd.equals("identify")) {
      Serial.print("BAD REQ\n");
      server.stop();
      return false;
    }
    else {
      server.print(identity);
      server.print("\n");
    }
  }
  Serial.print("Authenticated\n");
  
  if (persist) {
    if (server.connected()) {
		Serial.print("Persisting\n");
		return true;
	}
	else {
		Serial.print("FAIL\n");
		server.stop();
		return false;
	}
  }
  else {
	Serial.print("Disconnecting\n");
    server.stop();
    return true;
  }
}

/**
 * All nodes using the command interpreter should respond to a command that 
 * consts only of "____" with the response being non-blank.
 */
bool TcpClientRegistrar::probeConnection(WiFiClient& client, uint16_t timeout) {
  
  if (timeout != 0) {
	client.setTimeout(timeout);
  }
  
  client.flush();
  client.print("____\n");
  String response = client.readStringUntil('\n');
  
  return !response.equals("");
}





