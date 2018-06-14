/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Node:     1/Server (passive version, doesn't require other nodes)
 * Hardware: ESP8266-01[S]
 * Purpose:  Interface user commands, act as wireless access point.
 * Author:   Erik W. Greif
 * Date:     2018-06-13
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
#include <stdio.h>

//Custom libraries
#include <CommandInterpreter.h>
#include <TcpClientRegistrar.h>
#include <Pcf8591.h>

const char* WIFI_SSID = "SQUIRREL_NET";
const char* WIFI_PASS = "wj7n2-dx309-dt6qz-8t8dz";
IPAddress localIp(192,168,3,1);
IPAddress gateway(192,168,3,1);
IPAddress subnet(255,255,255,0);

const uint8_t MAX_AP_CLIENTS = 12;

//TCP connection pointers
WiFiClient* clientLaptop = NULL;

//Interpreters for user connections (clones, w/ separate buffers)
CommandInterpreter serialCmd;
CommandInterpreter laptopCmd;

//TCP server and the client id registrar: Handle reconnects seamlessly
WiFiServer listenSocket(23);
TcpClientRegistrar clients;

//Output indicator only
Pcf8591 ioChip(&Wire);

void setup() {
  //Turn wi-fi off (fix for soft reset)
  WiFi.mode(WIFI_OFF);
  delay(1000);
  
  Serial.begin(9600);
  delay(500);

  //Turn on wire library
  Wire.begin(2, 0);
  
  //Get wi-fi connected
  WiFi.softAPConfig(localIp, gateway, subnet);
  if (!WiFi.softAP(WIFI_SSID, WIFI_PASS)) {
    Serial.print("Critical failure!\n");
  }
  WiFi.mode(WIFI_AP);
  softAPSetMaxConnections(MAX_AP_CLIENTS);

  Serial.print("DEBUG: WiFi AP is ready\n");
  
  listenSocket.begin();
  Serial.print("DEBUG: Name server is ready\n");
  
  //Register local user commands to handler functions
  serialCmd.assignDefault(commandNotFound);
  serialCmd.assign("ip",          commandGetIp);
  serialCmd.assign("identify",    commandIdentify);
  serialCmd.assign("help",        commandHelp);
  serialCmd.assign("test-args",   commandTestArgs);
  serialCmd.assign("set-timeout", commandSetTimeout);
  serialCmd.assign("get-stats",   commandGetStats);
  
  //Allow mobile and laptop to do everything that the serial term can (copy)
  laptopCmd = CommandInterpreter(serialCmd);

  serialCmd.enableEcho(true);
  //serialCmd.enableSequenceNumbers(true);

  //Register client IDs to respective pointers for auto connection handling
  clients.assign("laptop", &clientLaptop);
}

void loop() {

  clients.handle(listenSocket);
  
  //Handle dispatching commands from various sources if they are available
  serialCmd.handle(Serial);
  if (clientLaptop)
    laptopCmd.handle(*clientLaptop);

  handleHeartbeat();
}

//******************************************************************************
//    MAINTENANCE LOOPS
//******************************************************************************

/**
 * Blinks the output LED at the given rate
 */
void handleHeartbeat() {
  static uint32_t aliveIndicateTime = 0;
  
  //Blink the LED on AOut by toggling from output to hi-z mode
  if (millis() - aliveIndicateTime > 2000) {
    aliveIndicateTime = millis();
    ioChip.write(0, 255, !ioChip.getOutputEnabled());
  }
}

//******************************************************************************
//    LOCAL COMMAND HANDLERS
//******************************************************************************

void commandHelp(Stream& port, int argc, const char** argv) {
  port.print("/======================================\\\n");
  port.print("|       Squirrel Lighting Server       |\n");
  port.print("\\======================================/\n");
  port.print("\nCommand Help\n\n");
  port.print("ip .................. Get IP by identity\n");
  port.print("identify ............ Get net identity\n");
  port.print("help ................ Command syntax manual\n");
  port.print("test-args ........... Test argument parser\n");
  port.print("set-timeout ......... Connection command timeout\n");
  port.print("get-stats ........... Get send/recv counters\n");
  port.print("drop-remote ......... Kill remote connections\n");
  port.print("\n");
  port.flush();
}

void commandIdentify(Stream& port, int argc, const char** argv) {
  port.print("squirrel\n");
}

void commandNotFound(Stream& port, int argc, const char** argv) {
  port.print("Unknown command\n");
}

void commandGetIp(Stream& port, int argc, const char** argv) {
  if (argc <= 0) {
    String toPrint = WiFi.softAPIP().toString() + "\n";
    port.print(toPrint);
    port.flush();
  }
  else {
    String toPrint = clients.findIp(argv[0]).toString() + "\n";
    port.print(toPrint);
    port.flush();
  }
}

void commandTestArgs(Stream& port, int argc, const char** argv) {
  for (int i = 0; i < argc; i++) {
    port.print("Argument ");
    port.print(i);
    port.print(": ");
    port.print(argv[i]);
    port.print("\n");
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
  Serial.print("mS\n");
  
  clients.setConnectionTimeout(timeout);
}

void commandGetStats(Stream& port, int argc, const char** argv) {

  long terminalToMaster = serialCmd.getReceiveCount();
  long masterToTerminal = serialCmd.getSendCount();

  port.printf("Send and Receive Stats:\n");
  port.printf("  Terminal to Squirrel:  %i\n", terminalToMaster);
  port.printf("  Squirrel to Terminal:  %i\n", masterToTerminal);
  port.flush();
}

