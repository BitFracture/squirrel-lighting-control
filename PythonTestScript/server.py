import socket;
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM);
sock.bind(("0.0.0.0", 2000));
sock.listen(5);
sock.settimeout(0.2)

def handleClient(client, addr):
    print("Client Sent: " + client.recv(7).decode("UTF-8"));
    client.close();

while True:
    try:
        try:
            client, addr = sock.accept();
            handleClient(client, addr);
            client.close();
        except socket.timeout as e:
            pass; # Do nothing
    except KeyboardInterrupt as e1:
        break;