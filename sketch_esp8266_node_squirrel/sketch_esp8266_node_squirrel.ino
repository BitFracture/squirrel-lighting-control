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
#include <stdio.h>

//Custom libraries
#include <CommandInterpreter.h>
#include <TcpClientRegistrar.h>
#include <Pcf8591.h>
#include <UdpStream.h>

const char* WIFI_SSID = "SQUIRREL_NET";
const char* WIFI_PASS = "wj7n2-dx309-dt6qz-8t8dz";
IPAddress localIp(192,168,3,1);
IPAddress gateway(192,168,3,1);
IPAddress subnet(255,255,255,0);

const uint8_t MAX_AP_CLIENTS = 12;

//TCP connection pointers
WiFiClient* clientMobile    = NULL;
WiFiClient* clientLaptop    = NULL;
WiFiClient* clientIoControl = NULL;
WiFiUDP clientRemoteDebug;
UdpStream outboundIoControl;
UdpStream inboundIoControl;

//Interpreters for user connections (clones, w/ separate buffers)
CommandInterpreter serialCmd;
CommandInterpreter mobileCmd;
CommandInterpreter laptopCmd;
CommandInterpreter ioCmd;
CommandInterpreter remoteDebugCmd;

//TCP server and the client id registrar: Handle reconnects seamlessly
WiFiServer listenSocket(23);
TcpClientRegistrar clients;

//Output indicator only
Pcf8591 ioChip(&Wire);

UdpStream serv;
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

  clientRemoteDebug.begin(24);

  //iocontrol commands
  ioCmd.assignDefault(commandNotFound);
  ioCmd.assign("ip", commandGetIp);

  //Register local user commands to handler functions
  serialCmd.assignDefault(commandNotFound);
  serialCmd.assign("ip",          commandGetIp);
  serialCmd.assign("identify",    commandIdentify);
  serialCmd.assign("help",        commandHelp);
  serialCmd.assign("test-args",   commandTestArgs);
  serialCmd.assign("set-timeout", commandSetTimeout);
  serialCmd.assign("get-stats",   commandGetStats);

  //Remote user commands (proxy them to IOControl)
  serialCmd.assign("color",          onSetColor);
  serialCmd.assign("get-color",      onGetColor);
  serialCmd.assign("temp",           onSetTemp);
  serialCmd.assign("get-temp",       onGetTemp);
  serialCmd.assign("brightness",     onSetBrightness);
  serialCmd.assign("get-brightness", onGetBrightness);
  serialCmd.assign("clap",           onSetClap);
  serialCmd.assign("get-clap",       onGetClap);
  serialCmd.assign("power",          onSetPower);
  serialCmd.assign("get-power",      onGetPower);
  serialCmd.assign("motion",         onSetMotion);
  serialCmd.assign("get-motion",     onGetMotion);
  serialCmd.assign("get-mode",       onGetMode);
  serialCmd.assign("listen",         onSetListen);
  serialCmd.assign("get-debug",      onGetDebug);
  
  //Allow mobile and laptop to do everything that the serial term can (copy)
  mobileCmd = CommandInterpreter(serialCmd);
  laptopCmd = CommandInterpreter(serialCmd);

  serialCmd.enableEcho(true);
  serialCmd.enableSequenceNumbers(true);

  //Allow remote nodes to send debug output through our serial on UDP
  remoteDebugCmd.assign("debug", commandRemoteDebug);

  //Register client IDs to respective pointers for auto connection handling
  clients.assign("laptop", &clientLaptop);
  clients.assign("mobile", &clientMobile);
  clients.assign("iocontrol", &clientIoControl);

  Serial.printf("Listening to IO Control, state is %i\n", inboundIoControl.begin(201));
}

void loop() {

  handleReconnect();
  
  clients.handle(listenSocket);
  
  //Handle dispatching commands from various sources if they are available
  serialCmd.handle(Serial);
  remoteDebugCmd.handleUdp(clientRemoteDebug);
  if (clientMobile)
    mobileCmd.handle(*clientMobile);
  if (clientLaptop)
    mobileCmd.handle(*clientLaptop);
  //if (clientIoControl)
  //  ioCmd.handle(*clientIoControl);
  ioCmd.handle(inboundIoControl);

  handleHeartbeat();
}

//******************************************************************************
//    MAINTENANCE LOOPS
//******************************************************************************

/**
 *  Handles basic reconnect operations
 */
void handleReconnect() {

  static int ioReconnectTimeout = 0;
  if (!outboundIoControl.connected() && millis() - ioReconnectTimeout > 2000) {
    ioReconnectTimeout = millis();
    Serial.print("Attempting connect to IOControl\n");
    
    if (outboundIoControl.begin(clients.findIp("iocontrol"), 200))
      Serial.print("Successfully connected to IOControl\n");
  }
}

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
//    REMOTE COMMAND HANDLERS
//******************************************************************************

/**
 * Reused by many handler functions to invoke remote commands without too much logic.
 */
void simpleProxyHandler(Stream& port, int argc, const char** argv, char* command, UdpStream& remote, int minArgs = 0, int maxArgs = 0) {

  if (argc < minArgs || argc > maxArgs) {
    if (minArgs == maxArgs)
      port.printf("ER: Requires exactly %i arguments\n", minArgs);
    else
      port.printf("ER: Requires %i to %i arguments\n", minArgs, maxArgs);
    port.flush();
    return;
  }

  if (!remote.connected()) {
    port.print("ER: Remote node is offline\n");
    port.flush();
    return;
  }

  String toPrint = command;
  for (int i = 0; i < argc; i++) {
    toPrint += String(" ") + String(argv[i]);
  }
  remote.printf("%s\n", toPrint.c_str());
  remote.flush();

  String response = remote.readStringUntil('\n');
  if (response.length() == 0) {

    port.print("ER: Remote node timed out\n");
    port.flush();
    return;
  }

  port.printf("%s\n", response.c_str());
}

void onSetColor(Stream& port, int argc, const char** argv) {

  if (argc != 1 && argc != 3) {
    port.print("ER: Requires 1 or 3 arguments\n");
    port.flush();
    return;
  }
  
  simpleProxyHandler(port, argc, argv, "color", outboundIoControl, 1, 3);
}
void onGetColor(Stream& port, int argc, const char** argv) {
  simpleProxyHandler(port, argc, argv, "get-color", outboundIoControl);
}
void onSetTemp(Stream& port, int argc, const char** argv) {
  simpleProxyHandler(port, argc, argv, "temp", outboundIoControl, 1, 1);
}
void onGetTemp(Stream& port, int argc, const char** argv) {
  simpleProxyHandler(port, argc, argv, "get-temp", outboundIoControl);
}
void onSetBrightness(Stream& port, int argc, const char** argv) {
  simpleProxyHandler(port, argc, argv, "brightness", outboundIoControl, 1, 1);
}
void onGetBrightness(Stream& port, int argc, const char** argv) {
  simpleProxyHandler(port, argc, argv, "get-brightness", outboundIoControl);
}
void onSetClap(Stream& port, int argc, const char** argv) {
  simpleProxyHandler(port, argc, argv, "clap", outboundIoControl, 1, 1);
}
void onGetClap(Stream& port, int argc, const char** argv) {
  simpleProxyHandler(port, argc, argv, "get-clap", outboundIoControl);
}
void onSetPower(Stream& port, int argc, const char** argv) {
  simpleProxyHandler(port, argc, argv, "power", outboundIoControl, 1, 1);
}
void onGetPower(Stream& port, int argc, const char** argv) {
  simpleProxyHandler(port, argc, argv, "get-power", outboundIoControl);
}
void onSetMotion(Stream& port, int argc, const char** argv) {
  simpleProxyHandler(port, argc, argv, "motion", outboundIoControl, 1, 1);
}
void onGetMotion(Stream& port, int argc, const char** argv) {
  simpleProxyHandler(port, argc, argv, "get-motion", outboundIoControl);
}
void onGetMode(Stream& port, int argc, const char** argv) {
  simpleProxyHandler(port, argc, argv, "get-mode", outboundIoControl);
}
void onSetListen(Stream& port, int argc, const char** argv) {
  simpleProxyHandler(port, argc, argv, "listen", outboundIoControl);
}
void onGetDebug(Stream& port, int argc, const char** argv) {
  simpleProxyHandler(port, argc, argv, "get-debug", outboundIoControl);
}

//******************************************************************************
//    LOCAL COMMAND HANDLERS
//******************************************************************************

void commandHelp(Stream& port, int argc, const char** argv) {
  port.print("/======================================\\\n");
  port.print("|       Squirrel Lighting Server       |\n");
  port.print("\\======================================/\n");
  port.print("\n");
  port.print("User Command Help\n");
  port.print("\n");
  port.print("> ip ............ Get IP by identity\n");
  port.print("> identify ...... Get net identity\n");
  port.print("> help .......... Command syntax\n");
  port.print("> test-args ..... Test argument parser\n");
  port.print("> set-timeout ... Millis to respond to\n");
  port.print("                  connection commands\n");
  port.print("\n");
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
  long masterToSlave    = outboundIoControl.getSendCount();
  long slaveToMaster    = outboundIoControl.getReceiveCount();
  //mobileCmd
  //laptopCmd
  port.printf("Send and Receive Stats:\n");
  port.printf("  Terminal to Squirrel:  %i\n", terminalToMaster);
  port.printf("  Squirrel to Terminal:  %i\n", masterToTerminal);
  port.printf("  Squirrel to IOControl: %i\n", masterToSlave);
  port.printf("  IOControl to Squirrel: %i\n", slaveToMaster);
  port.flush();
}

/**
 * Takes remote data and outputs it to the command prompt at this node.
 */
void commandRemoteDebug(Stream& port, int argc, const char** argv) {

  for (int ar = 0; ar < argc; ar++)
  {
    Serial.print("DEBUG (remote): ");
    Serial.print(argv[ar]);
    Serial.print("\n");
  }

  port.print("OK\n");
}

