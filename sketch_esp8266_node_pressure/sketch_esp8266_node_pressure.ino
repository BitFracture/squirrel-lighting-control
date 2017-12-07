/**
   The Flying Squirrels: Squirrel Lighting Controller
   Node:     Client: Pressure Meter
   Hardware: ESP8266-01[S]
   Purpose:  Monitor pressure sensor levels, report to server.
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
#include <UdpStream.h>
#include <Pcf8591.h>

const char* WIFI_SSID = "SQUIRREL_NET";
const char* WIFI_PASS = "wj7n2-dx309-dt6qz-8t8dz";
bool reconnect = true;

WiFiEventHandler disconnectedEventHandler;
CommandInterpreter ioCmd;
UdpStream inboundIoControl;
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
}


void loop() {

  //Do nothing until we are connected to the server
  handleReconnect();

  //Handle commands
  ioCmd.handle(Serial);
  ioCmd.handle(inboundIoControl);
  
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

    //Open the UDP socket
    inboundIoControl.begin(400);
  }
}


void getPressure(Stream& reply, int argc, const char** argv) {
  static const int BUFFER_LEN = 5;
  static char buffer[BUFFER_LEN];
  sprintf(buffer, "%i\n", ioChip.read(0, 0));
  reply.print(buffer);
}


