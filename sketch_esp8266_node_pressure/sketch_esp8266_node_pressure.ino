/**
   The Flying Squirrels: Squirrel Lighting Controller
   Node:     2/Client: Sun Meter
   Hardware: ESP8266-01[S]
   Purpose:  Monitor exterior light levels, report to server.
   Author:   Erik W. Greif
   Date:     2017-10-14
*/


#include <ESP8266WiFi.h>
#include <ESP8266WiFiAP.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFiScan.h>
#include <ESP8266WiFiSTA.h>
#include <ESP8266WiFiType.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>

#include <TcpClientRegistrar.h>
#include <CommandInterpreter.h>
#include <Pcf8591.h>
#include <UdpStream.h>

const char* WIFI_SSID = "SQUIRREL_NET";
const char* WIFI_PASS = "wj7n2-dx309-dt6qz-8t8dz";
bool reconnect = true;

WiFiEventHandler disconnectedEventHandler;
TcpClientRegistrar registrar;
CommandInterpreter ioCmd;
WiFiClient* clientIoControl = NULL;
WiFiServer listeningConnection(23);
UdpStream inboundSquirrel;
UdpStream outboundSquirrel;
Pcf8591 ioChip(&Wire);

void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.print("Initialized\n");
  Wire.begin(2, 0);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  disconnectedEventHandler = WiFi.onStationModeDisconnected(&triggerReconnect);

  ioCmd.assign("g", getPressure);
  ioCmd.assign("ip", getIpFromSquirrel);

  listeningConnection.begin();
  registrar.assign("iocontrol", &clientIoControl);
}

void loop() {
//*
  /*uint8_t connectState = 0;
  while (!connectState) {
    connectState = clientSock.begin(IPAddress(192,168,3,1), 40);
    Serial.printf("Connecting got %i\n", connectState);
  }
  while (true) {
    //delay(500);
    Serial.println("-----\nSending data...");
    clientSock.printf("Some data\n");
    clientSock.flush();
    clientSock.setTimeout(2000);
    String data = clientSock.readStringUntil('\n');
    Serial.printf("Got reply \"%s\"\n", data.c_str());
  }*/
//*/
  //Do nothing until we are connected to the server
  handleReconnect();

  // Client recv
  registrar.handle(listeningConnection);

  //Handle commands
  ioCmd.handle(Serial);
  if (clientIoControl != NULL && clientIoControl->connected())
    ioCmd.handle(*clientIoControl);
  
  handleHeartbeat();
}

/**
 * Blinks the output LED at the given rate.
 */
void handleHeartbeat() {
  static uint32_t aliveIndicateTime = 0;
  
  //Blink the LED on AOut by toggling from output to hi-z mode
  if (millis() - aliveIndicateTime > 2000) {
    aliveIndicateTime = millis();
    ioChip.write(0, 255, !ioChip.getOutputEnabled());
  }
}

void triggerReconnect(const WiFiEventStationModeDisconnected& event) {

  reconnect = true;
}

void handleReconnect() {
  //TODO: Reconnect if server TCP connection lost
  while (reconnect) {
    //Wait for wifi for 5 seconds
    Serial.print("Wait\n");
    for (int i = 10; WiFi.status() != WL_CONNECTED && i > 0; i--) {
      delay(500);
    }
    if (WiFi.status() != WL_CONNECTED) {
      continue;
    }

    //Try to connect persistently to squirrel
    WiFiClient registerClient;
    if (TcpClientRegistrar::connectClient(
          registerClient, IPAddress(192, 168, 3, 1), 23, "pressure", false))
      reconnect = false;

    //Connect UDP to squirrel as a client
    uint8_t connectState = 0;
    while (!connectState) {
      connectState = outboundSquirrel.begin(IPAddress(192,168,3,1), 201);
      Serial.print(connectState ? "Connect to Squirrel succeeded\n" : "UDP to Squirrel failed\n");
    }
  }
}

void getPressure(Stream& reply, int argc, const char** argv) {
  static const int BUFFER_LEN = 5;
  static char buffer[BUFFER_LEN];
  sprintf(buffer, "%i\n", ioChip.read(0, 0));
  reply.print(buffer);
}

void getIpFromSquirrel(Stream& reply, int argc, const char** argv) {

  if (!outboundSquirrel.connected()) {
    reply.print("Connection to Squirrel is inactive\n");
    reply.flush();
    return;
  }

  if (argc != 1) {
    reply.print("Command accepts exactly 1 argument\n");
    reply.flush();
    return;
  }

  outboundSquirrel.printf("ip %s\n", argv[0]);
  outboundSquirrel.flush();
  String response = outboundSquirrel.readStringUntil('\n');

  reply.printf("Squirrel says \"%s\"\n", response.c_str());
  reply.flush();
}

