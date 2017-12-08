import socket;
import sys;
import time;
import random

okRsp = b'OK\n'
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
print("A")
sock.connect(("192.168.3.4", 900))
print("B")
sock.settimeout(1);

commands = [b'persist\n', b'laptop\n'];
for cmd in commands:
  print(">" + repr(sock.recv(1024)))
  sock.send(cmd)
  print("<" + repr(cmd))

color = [0,0,0]

MODE_POS = {'color': 0, 'temp': 1, 'listen': 2, 'off': 3, 'yield': 4}
MODE_NAMES = [b"MODE_COLOR", b"MODE_TEMP", b"MODE_LISTEN", b"MODE_OFF", b"MODE_YIELD"]

temperature = 0

brightnessOn = False
brightness = 0

powerOn = True

stateCur = MODE_NAMES[MODE_POS['temp']];
statePrev = MODE_NAMES[MODE_POS['temp']];

def changeMode(modeName):
  global statePrev
  global stateCur
  global MODE_NAMES
  global MODE_POS
  statePrev = stateCur
  stateCur = MODE_NAMES[MODE_POS[modeName]]

# >b'mode\n'
# <b'persist\n'
# >b'identify\n'
# <b'laptop\n'
# <b'get-color\n'
# >b'0 0 0\n'
# <b'get-temp\n'
# >b'OFF\r\n'
# <b'get-brightness\n'
# >b'0\n'
# <b'get-clap\n'
# >b'OFF\r\n'
# <b'get-power\n'
# >b'OFF\r\n'
# <b'get-motion\n'
# >b'OFF\r\n'
# <b'get-mode\n'
# >b'MODE_TEMP\n'
# >

def getColor():
  global statePrev
  global stateCur
  global color
  if color[0] < 0 or color[1] < 0 or color[2] < 0:
    return b'auto'
  else:
    return bytearray(str(color[0]) + " " + str(color[1]) + " " + str(color[2]), "UTF-8")

def getTemp():
  global temperature
  if temperature < 0:
    return b'auto'
  else:
    return bytearray(str(temperature), "ASCII")

def getBrightness():
  global brightnessOn
  if brightnessOn:
    return bytearray(str(brightness), "ASCII")
  else:
    return b'OFF'

def getClap():
  return b'OFF'

def getPower():
  if powerOn:
    return b'ON'
  else:
    return b'OFF'

def getMotion():
  return b'OFF'

def getMode():
  return stateCur

commands = [
  [b'get-color', getColor],
  [b'get-temp', getTemp],
  [b'get-brightness', getBrightness],
  [b'get-clap', getClap],
  [b'get-power', getPower],
  [b'get-motion', getMotion],
  [b'get-mode', getMode]
];

def checkState():
  global commands
  for cmd in commands:
    sock.send(cmd[0] + b'\n')
    rsp = sock.recv(1024)
    print("<" + repr(cmd[0] + b'\n'))
    print(">" + repr(rsp))
    if rsp != cmd[1]() + b'\n':
      return False
    else:
      return True

def setRndColor():
  global color
  color = [random.randint(-10, 255), random.randint(-10, 255), random.randint(-10, 255)]
  cmd = b'color ' + getColor() + b'\n'
  sock.send(cmd)
  rsp = sock.recv(1024)
  print("<" + repr(cmd))
  print(">" + repr(rsp))
  if rsp != okRsp:
    return False
  else:
    changeMode('color')
    return True

def setRndTemp():
  global temperature
  temperature = random.randint(-10, 255)
  cmd = b'temp ' + getTemp() + b'\n'
  sock.send(cmd)
  rsp = sock.recv(1024)
  print("<" + repr(cmd))
  print(">" + repr(rsp))
  if rsp != okRsp:
    return False
  else:
    changeMode('temp')
    return True

def setRndBrightness():
  global temperature
  temperature = random.randint(-10, 255)
  cmd = b'temp ' + getTemp() + b'\n'
  sock.send(cmd)
  rsp = sock.recv(1024)
  print("<" + repr(cmd))
  print(">" + repr(rsp))
  if rsp != okRsp:
    return False
  else:
    return True

# testCases = [setRndColor, setRndTemp, setRndBrightness]
# for i in range(1):
  # testCases[random.randint(0, len(testCases) - 1)]()

# while True:
  # sock.send(b'get-debug\n')
  # print(sock.recv(1024).decode("UTF-8").strip())
  # time.sleep(0.01)

while True:
  bOutput = bytearray(input(">") + "\n", "UTF-8")
  
  if (bOutput[0] == 49): #b'1'
    setRndColor()
  elif (bOutput[0] == 50):
    setRndTemp()
  else:
    sock.send(bOutput)
    try:
      print(sock.recv(1024))
    except socket.timeout as e:
      pass

sock.close()

# sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM);
# sock.bind(("0.0.0.0", 80));
# sock.listen(5);
# sock.settimeout(0.2);
# BUFFER_SIZE = 1024
# commands = ['identify', 'mode', "test"];
# print("Listening to port " + str(sock.getsockname()));


# def recvFromClient(client):
    # buffer = ""
    # while True:
      # try:
            # data = client.recv(BUFFER_SIZE).decode("UTF-8")
            # buffer += data;
            # if len(data) < BUFFER_SIZE:
                # break;
      # except socket.timeout as e:
          # pass; # Do nothing
    
    # return buffer;

# def handleClient(client, addr):
    # for cmd in commands:
        # print("Sending Client: " + cmd);
        # client.send(bytearray(cmd, "UTF-8"));
        # print("Client returned: " + repr(recvFromClient(client)));
    
    # print("Client connected");
    
    # client.settimeout(2);
    # while True:
        # msg = recvFromClient(client);
        # print("Client returned: " + repr(msg));
        # if (len(msg) == 0): break;
        # if (msg == "sin"):
          # client.send(bytearray("ack", "UTF-8"));
      
    # client.close();
    # print("Done handling client");
# while True:
    # try:
        # try:
            # client, addr = sock.accept();
            # handleClient(client, addr);
            # client.close();
        # except socket.timeout as e:
            # pass; # Do nothing
    # except KeyboardInterrupt as e1:
        # break;
        