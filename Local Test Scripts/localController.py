"""
A local light controller (IO Control) for a workstation. For use with the PASSIVE hub only!

Author: Erik W. Greif
Date: 2018-06-13
"""

import socket

UDP_IP = "0.0.0.0"
UDP_PORT = 23

sock = socket.socket(socket.AF_INET, # Internet
                     socket.SOCK_DGRAM) # UDP
sock.bind((UDP_IP, UDP_PORT))

while True:
    print ("Waiting to receive...")
    (data, addr) = sock.recvfrom(128) # buffer size is 128 bytes
    if data == b"d":
        print ("{} is asking to be paired".format(addr[0]))
