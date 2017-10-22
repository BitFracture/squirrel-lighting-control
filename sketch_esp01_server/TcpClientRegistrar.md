
# TCP Client Registrar for Arduino/ESP8266

This library is designed to simplify the process of networking TCP nodes in an ESP8266 network. Incoming clients must be able to respond to two simple commands, which allow this library to store its IP and Identity (virtual DNS). If a persistent connection is chosen by the connecting client, and the identity is in an approved list, a pointer is updated to point to the new client. This seamless replacement of pointers in the program space reduces all of the client connection processing to one handle() function. 


## Using Client Pointer Assignment

Start by defining a WiFiServer object to listen for connections, an instance of TcpClientRegistrar, and a pointer to a WiFiClient set to NULL. For our example, we are listening on TCP port 23 (typically TelNet).

```
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

WiFiClient* myClient = NULL;
WiFiServer listenSocket(23);
TcpClientRegistrar clients;
```

During your Setup(), you need to link a unique identifier to the pointer you defined. When a client connects, the library will ask for this identifier and link the client appropriately. 

```
void setup() {
    clients.assign("my_client_identifier", &myClient);
}
```

Lastly, you need to call the handle function in your loop. The performance of handle() depends largely on you leaving loop() free to cycle. If you use lots of time consuming calls, connections will not be handled until handle() is called. You may call handle() on multiple listening sockets. 

```
void loop() {
    clients.handle(listenSocket);
	...
```

You can now use the pointer myClient in your loop or elsewhere. Simply make sure to verify it is valid before use. 

```
    ...
	if (myClient && myClient.connected()) {
	    //Logic for myClient interaction here
	}
}
```


## Using The Registrar

When clients do not request a persistent connection, they are still logged into the registrar. This is handy if you have a network of nodes where you need to have a source of truth for IP addresses of certain devices. 

To retrieve the IP address of a registered client, call the registrar with the identifier you want the address for. 

```
uint32_t myClientIp = clients.findIp("my_client_identifier");
```

If you get back a non-0 value, the call was successful. 


## Being A Client

The other end to this story is how to be a client. A client will open a connection with the server, and the server will then issue the command "mode". It is the client's job to respond with "register" or "persist". 

Secondly, the server will issue the command "identify", and the client must respond with the unique identity we mentioned earlier. 

At this point, if you chose "register", your connection will be terminated. The server now keeps a record of your IP address. If you chose "persist" and you are a registered client (remember `assign(identifier, pointer)`) then your connection is ready for you to communicate normally. If your identity was never assigned with a pointer, you will be booted the same way you would if you had chosen "register". 

All commands and responses must be terminated with a newline (LF, \n), and the server is tolerant of carriage returns (CRLF, \r\n). 
