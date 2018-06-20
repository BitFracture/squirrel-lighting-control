"""
A local light controller (IO Control) for a workstation. For use with the PASSIVE hub only!

Author: Erik W. Greif
Date: 2018-06-13
"""

import socket
import sys
from multiprocessing import Pool
import multiprocessing
from asyncio import Lock
from time import sleep

UDP_IP = "0.0.0.0"
UDP_PORT = 23

def clientListenerMain(clientList, lock):
    """
    Separate thread or process to listen for new UDP clients sending "d" (discovery) packets.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))

    print("Subscription listener has started")
    sys.stdout.flush()

    while True:
        (data, addr) = sock.recvfrom(128) # buffer size is 128 bytes
        if data == b"d":
            print ("{} is asking to be paired".format(addr[0]))
            sys.stdout.flush();
            lock.acquire()
            clientList.append(addr[0])
            lock.release()

def clientMulticastMain(command, clientList, lock):
    """
    Rebroadcasts the most recent command periodically until a new command is issued
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    while (True):
        sleep(.9)
        lock.acquire()
        for address in clientList:
            sock.sendto(command["value"].encode('ascii'), (address, UDP_PORT))
        lock.release()

def clientMain(command, clientList, lock):
    """
    Main thread for sending data to registered clients
    """
    while True:
        print(" > ", end="")
        userString = input().strip()

        if (userString == "quit"):
            break;
        else:
            command["value"] = userString + "\n"

if __name__ == "__main__":
    print("Local Light Controller for Squirrel (passive mode)")

    # Thread safe structures
    manager = multiprocessing.Manager()
    lock = multiprocessing.Lock()
    clientList = manager.list()
    command = manager.dict()
    command["value"] = "t 128\n"

    # Start listener and sender main
    listener = multiprocessing.Process(target = clientListenerMain, args=(clientList, lock))
    listener.start()
    sender = multiprocessing.Process(target = clientMulticastMain, args=(command, clientList, lock))
    sender.start()
    clientMain(command, clientList, lock)

    # Once sender main terminates, kill the listener
    listener.terminate()
    sender.terminate()
    print("Waiting for subscription listener and sender to quit...")
    listener.join()
    sender.join()
