
# Description of Communication Protocols

## Nodes

Node0: squirrel  (server)  
Node1: daylight  (daylight sensor)  
Node2: iocontrol (motion, pressure, audio)  
Node3: lumen0    (RGBLED bulb)  
  
...  
  
Node4: laptop        (remote terminal)  
Node5: mobile        (remote terminal/mobile app)  
Virtual node: Serial (uart commands)  

We will summarize laptop, mobile, and Serial as a node called 'user'.

## Device Connections (wired or wireless)

This basically sums up which devices will be connected to which, once all
name resolving is settled. 

```
iocontrol --1-1--> squirrel
iocontrol --1-1--> daylight
iocontrol --1-N--> lumen[N]
user      --N-1--> squirrel
```

For example, if squirrel wants to know the daylight level, it must ask iocontrol to ask daylight for the value. 

## Establishing A Connection

1. TCP connect to 192.168.1.1
2. Server requests with "auth"
3. Respond with "persist" or "register"  
persist = Persistent connection. Server must be expecting or will boot.  
register = Supply identifying information and terminate.
4. Server requests with "identify"
5. Respond to an "identify" command with a unique identity.
6. If "persist" and server expects connection, connected. Ready.  
If "register", server notes identity to IP mapping. 
	
The register functionality allows the server to act as a domain name 
controller. Persistent clients are saved as well as registering clients.
Now, the common IP of *.1 can be used to fetch the IP of others by identity.

1. lumen0 connects to 192.168.1.1
2. squirrel requests "auth"
3. lumen0 responds "register"
4. squirrel requests "identify"
5. lumen0 responds "lumen0"
6. squirrel notes the identity and IP and terminates the connection
7. iocontrol TCP connects to 192.168.1.1
8. squirrel requests "auth"
9. iocontrol responds "persist"
10. squirrel requests "identify"
11. iocontrol responds "iocontrol"
12. iocontrol requests squirrel "ip lumen0"
13. squirrel responds "192.168.1.12"
14. iocontrol TCP connects to 192.168.1.12
15. lumen0 responds "auth"
16. iocontrol responds "register"
17. lumen0 requests "identify"
18. iocontrol responds "iocontrol"
19. iocontrol is now connected to lumen0

## User Instructions

```
power [on | off]             Turn on system or force light(s) to off state
brightness [auto | 0-255]    Motion/pressure derived or manual value
temperature [auto | 0-255]   Overrides color! By day sensor or manual value
listen [on | off]            Audio-based brightness diffs on or off
color 0-255 0-255 0-255      Overrides temp! Manual color value
calibrate ...                TBD calibration processes
```

## Known Data Flows

This list will translates commands through the network to the source of data.
List is not complete. 

```
SOURCE    DEST        COMMAND
---------------------------------
user      squirrel    power on
squirrel  iocontrol   power on
iocontrol lumen0      power on

user      squirrel    brightness auto
squirrel  iocontrol   brightness auto
[loop]
iocontrol daylight    get
iocontrol lumen0      temperature [value]
[endloop]
```

