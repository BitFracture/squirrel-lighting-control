/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Node:     1/Server
 * Hardware: ESP8266-01[S]
 * Purpose:  Interface user commands, act as wireless router.
 * Author:   Erik W. Greif
 * Date:     2017-10-13
 */

//ESP8266 libraries
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

//Custom libraries
#include <CommandInterpreter.h>
#include <TcpClientRegistrar.h>

const char* WIFI_SSID = "SQUIRREL_NET";
const char* WIFI_PASS = "wj7n2-dx309-dt6qz-8t8dz";
IPAddress localIp(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

const uint8_t MAX_AP_CLIENTS = 12;

//TCP connection pointers
WiFiClient* clientMobile    = NULL;
WiFiClient* clientLaptop    = NULL;
WiFiClient* clientIoControl = NULL;

//Interpreters for user connections (clones, w/ separate buffers)
CommandInterpreter serialCmd;
CommandInterpreter mobileCmd;
CommandInterpreter laptopCmd;

//TCP server and the client id registrar: Handle reconnects seamlessly
WiFiServer listenSocket(23);
TcpClientRegistrar clients;

void setup() {
  //Get wi-fi connected
  WiFi.softAPConfig(localIp, gateway, subnet);
  if (!WiFi.softAP(WIFI_SSID, WIFI_PASS)) {
    Serial.println("Critical failure!");
  }
  WiFi.mode(WIFI_AP);
  softAPSetMaxConnections(MAX_AP_CLIENTS);
  
  delay(500);
  Serial.begin(9600);
  Serial.println("DEBUG: WiFi AP is ready");

  listenSocket.begin();
  Serial.println("DEBUG: Server is ready");

  //Register user commands to handler functions
  serialCmd.assignDefault(commandNotFound);
  serialCmd.assign("diag", commandGetDiagnostics);
  serialCmd.assign("ip", commandGetIp);
  serialCmd.assign("scan", commandScanNetworks);
  serialCmd.assign("identify", commandIdentify);
  serialCmd.assign("help", commandHelp);
  serialCmd.assign("test-args", commandTestArgs);
  serialCmd.assign("set-timeout", commandSetTimeout);
  
  //Allow mobile and laptop to do everything that the serial term can (copy)
  mobileCmd = CommandInterpreter(serialCmd);
  laptopCmd = CommandInterpreter(serialCmd);

  //Register client IDs to respective pointers for auto connection handling
  clients.assign("laptop", &clientLaptop);
  clients.assign("mobile", &clientMobile);
  clients.assign("iocontrol", &clientIoControl);
}

void loop() {
  clients.handle(listenSocket);
  
  //Handle dispatching commands from various sources if they are available
  serialCmd.handle(Serial);
  if (clientMobile)
    mobileCmd.handle(*clientMobile);
  if (clientLaptop)
    mobileCmd.handle(*clientLaptop);
}

void commandNotFound(Stream& port, int argc, const char** argv) {
  port.println("Unknown command");
}

void commandGetDiagnostics(Stream& port, int argc, const char** argv) {
  WiFi.printDiag(port);
}

void commandGetIp(Stream& port, int argc, const char** argv) {
  if (argc <= 0)
    port.println(WiFi.softAPIP());
  else {
    port.println(clients.findIp(argv[0]));
  }
}

void commandScanNetworks(Stream& port, int argc, const char** argv) {
  int numNetworks = WiFi.scanNetworks();
  for (int i = 0; i < numNetworks; i++) {
    port.print(WiFi.SSID(i));
    port.print(" (");
    port.print(WiFi.RSSI(i));
    port.print(", ");
    port.print(WiFi.channel(i));
    port.println(")");
  }
}

void commandIdentify(Stream& port, int argc, const char** argv) {
  port.println("squirrel");
}

void commandHelp(Stream& port, int argc, const char** argv) {
  port.println("/======================================\\");
  port.println("|       Squirrel Lighting Server       |");
  port.println("\\======================================/");
  port.println("");
  port.println("User Command Help");
  port.println("");
  port.println("> diag .......... ESP diagnostic report");
  port.println("> ip ............ Get server IP");
  port.println("> scan .......... Scan available wifi");
  port.println("> identify ...... Get net identity");
  port.println("> help .......... Command syntax");
  port.println("> test-args ..... Test argument parser");
  port.println("> set-timeout ... Millis to respond to");
  port.println("                  connection commands");
  port.println("");
}

void commandTestArgs(Stream& port, int argc, const char** argv) {
  for (int i = 0; i < argc; i++) {
    port.print("Argument ");
    port.print(i);
    port.print(": ");
    port.println(argv[i]);
  }
}

void commandSetTimeout(Stream& port, int argc, const char** argv) {
  if (argc <= 0)
    return;

  int timeout = (String(argv[0])).toInt();
  timeout = timeout < 0 ? 0 : timeout;
  
  Serial.print("DEBUG: Changed connection timeout from ");
  Serial.print(clients.getConnectionTimeout());
  Serial.print("mS to ");
  Serial.print(timeout);
  Serial.println("mS");
  
  clients.setConnectionTimeout(timeout);
}

