import socket;

remote = (input("Address: "), int(input("Port: ")))
while True:
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM);
        sock.connect(remote);
        sock.send(bytearray("0123456", "UTF-8"));
        sock.close();
    except KeyboardInterrupt as e:
        break;

print("Bye");