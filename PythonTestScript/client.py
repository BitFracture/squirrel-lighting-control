import socket;
import time;

remote = ("192.168.1.100", 23)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM);
sock.connect(remote);

print(sock.recv(10000));
sock.send(bytearray("persist\n", "UTF-8"));

print(sock.recv(10000));
sock.send(bytearray("iocontrol\n", "UTF-8"));

while True:
    for i in range(255):
        sock.send(bytearray("s 0 0 0 " + str(i) + "\n", "UTF-8"));
        print(i);
        time.sleep(0.001);

    for i in reversed(range(254)):
        sock.send(bytearray("s 0 0 0 " + str(i) + "\n", "UTF-8"));
        print(i);
        time.sleep(0.001);

sock.close();