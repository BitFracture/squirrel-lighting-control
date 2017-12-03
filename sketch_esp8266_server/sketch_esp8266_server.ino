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
#include <WiFiClientReliable.h>
#include <WiFiServerReliable.h>

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
  Serial.print("DEBUG: Server is ready\n");

  clientRemoteDebug.begin(24);

  //iocontrol commands
  ioCmd.assignDefault(commandNotFound);
  ioCmd.assign("ip", commandGetIp);

  //Register user commands to handler functions
  serialCmd.assignDefault(commandNotFound);
  serialCmd.assign("ip", commandGetIp);
  serialCmd.assign("identify", commandIdentify);
  serialCmd.assign("help", commandHelp);
  serialCmd.assign("test-args", commandTestArgs);
  serialCmd.assign("set-timeout", commandSetTimeout);
  serialCmd.assign("color", commandSetColor);
  serialCmd.assign("temp", commandSetTemp);
  serialCmd.assign("test", commandTest);
  
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
}

void loop() {

  //HIJACK LOOP
/*  while (true) {
    delay(500);

    Serial.println("Server starting");
    WiFiServerReliable serv(30);

    while (!serv.hasClient()) {
      delay(500);
    }
    
    Serial.println("Got a client");
    WiFiClientReliable cli = serv.available();
    cli.print("LLAMAS\n");
    cli.stop();
    
    Serial.println("Sent and closing");
  }*/
  
  clients.handle(listenSocket);
  
  //Handle dispatching commands from various sources if they are available
  serialCmd.handle(Serial);
  remoteDebugCmd.handleUdp(clientRemoteDebug);
  if (clientMobile)
    mobileCmd.handle(*clientMobile);
  if (clientLaptop)
    mobileCmd.handle(*clientLaptop);
  if (clientIoControl)
    ioCmd.handle(*clientIoControl);

  handleHeartbeat();
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

void commandNotFound(Stream& port, int argc, const char** argv) {
  port.print("Unknown command\n");
}

void commandGetDiagnostics(Stream& port, int argc, const char** argv) {
  WiFi.printDiag(port);
}

void commandGetIp(Stream& port, int argc, const char** argv) {
  if (argc <= 0) {
    String toPrint = WiFi.softAPIP().toString() + "\n";
    port.print(toPrint);
  }
  else {
    String toPrint = clients.findIp(argv[0]).toString() + "\n";
    port.print(toPrint);
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
    port.print(")\n");
  }
}

void commandIdentify(Stream& port, int argc, const char** argv) {
  port.print("squirrel\n");
}

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

char cmdOutBuffer[15];
void commandSetColor(Stream& port, int argc, const char** argv) {

  if (!clientIoControl || !clientIoControl->connected())
  {
    port.print("Waiting for IOControl connection, please try again later");
    return;
  }
  
  if (argc == 1 && strcmp("auto", argv[0]) == 0) {
    sprintf(cmdOutBuffer, "c a\n");
  } else if (argc == 3) {
    sprintf(cmdOutBuffer, "c %s %s %s\n", argv[0], argv[1], argv[2]);
  } else {
    port.print("You must specify 3 space-delimited color channels 0 to 255 or auto\n");
    return;
  }
  
  clientIoControl->print(&cmdOutBuffer[0]);
  port.print("OK\n");
}

void commandSetTemp(Stream& port, int argc, const char** argv) {
  if (argc != 1) {
    
    port.print("You must specify 1 color channel 0 to 255\n");
    return;
  }

  if (!clientIoControl || !clientIoControl->connected())
  {
    port.print("Waiting for IOControl connection, please try again later");
    return;
  }

  if (strcmp("auto", argv[0]) == 0) {
    sprintf(cmdOutBuffer, "t a\n");
  } else {
    sprintf(cmdOutBuffer, "t %s\n", argv[0]);
  }

  clientIoControl->print(&cmdOutBuffer[0]);
  port.print("OK\n");
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

void commandTest(Stream& port, int argc, const char** argv) {

  port.print("this-is");
  port.print("-goat\n");
  port.setTimeout(2000);
  String response = port.readStringUntil('\n');
  port.printf("received %s\n", response.c_str());
}



