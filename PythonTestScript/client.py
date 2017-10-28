import socket;
import time;

# Connect to server to figure out where lumen0 is
serverSock = socket.socket(socket.AF_INET, socket.SOCK_STREAM);
serverSock.connect(("192.168.3.1", 23));

time.sleep(.025)
print(serverSock.recv(256));
serverSock.send(bytearray("persist\n", "UTF-8"));
time.sleep(.025);
print(serverSock.recv(256));
serverSock.send(bytearray("iocontrol\n", "UTF-8"));

time.sleep(.025);
serverSock.send(bytearray("ip lumen0\n", "UTF-8"));
time.sleep(.5);
lumen0Ip = serverSock.recv(256).replace(b"\n", b"");
print(lumen0Ip);

# Connect to lumen0
lightSock = socket.socket(socket.AF_INET, socket.SOCK_STREAM);
lightSock.connect((lumen0Ip, 23));

time.sleep(.025)
print(lightSock.recv(256));
lightSock.send(bytearray("persist\n", "UTF-8"));
time.sleep(.025);
print(lightSock.recv(256));
lightSock.send(bytearray("iocontrol\n", "UTF-8"));

# Send some color choices
while True:
    for q in range(20480):
        i = (q // 10) % 255;
        j = (q // 10 + 85) % 255;
        k = (q // 10 + 170) % 255;
        lightSock.send(bytearray("s " + str(i) + " " + str(j) + " " + str(k) + " 0\n", "UTF-8"));
        print(lightSock.recv(256));
        print(i);
        time.sleep(0.0005);

serverSock.close();
lightSock.close();