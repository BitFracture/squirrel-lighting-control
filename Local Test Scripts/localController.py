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
import json
import datetime

UDP_IP = "0.0.0.0"
UDP_PORT = 65500

def clientListenerMain(clientList, lock):
    """
    Separate thread or process to listen for new UDP clients sending "d" (discovery) packets.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))

    print("Subscription listener has started")
    sys.stdout.flush()

    while True:
        (rawData, addr) = sock.recvfrom(512) # buffer size is 128 bytes
        try:
            data = json.loads(rawData)
        except:
            data = {}

        if data.get("firmware", "") != "squirrel":
            continue;

        if data.get("action", "") == "discover":
            lock.acquire()
            pairClient(clientList, CreateClientEntry(data.get("name", ""), addr[0]))
            lock.release()
        elif data.get("action", "") == "keep-alive":
            lock.acquire()
            renewClient(clientList, CreateClientEntry(data.get("name", ""), addr[0]))
            lock.release()

def CreateClientEntry(name, ip):
    return {
        "name": name,
        "ip": ip,
        "time": datetime.datetime.now().timestamp()
    }

def removeClient(clientList, client):
    for i in range(0, len(clientList)):
        resource = json.loads(clientList[i])
        if resource.get("ip", "") == client.get("ip", ""):
            clientList.pop(i)
            print("Removing \"{}\" at IP {}".format(client.get("name", ""), client.get("ip", "")))
            sys.stdout.flush()
            break

def pairClient(clientList, client):
    removeClient(clientList, client)
    clientList.append(json.dumps(client))
    print("Pairing \"{}\" at IP {}".format(client.get("name", ""), client.get("ip", "")))
    sys.stdout.flush()

def renewClient(clientList, client):
    for i in range(0, len(clientList)):
        resource = json.loads(clientList[i])
        if resource.get("ip", "") == client.get("ip", ""):
            clientList[i] = json.dumps(client)
            print("Renewing \"{}\" at IP {}".format(client.get("name", ""), client.get("ip", "")))
            sys.stdout.flush()
            break

def clientMulticastMain(command, clientList, lock):
    """
    Rebroadcasts the most recent command periodically until a new command is issued
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    while (True):
        sleep(.9)
        threshold = datetime.datetime.now().timestamp() - 90
        lock.acquire()
        for clientSerial in clientList:
            client = json.loads(clientSerial)
            if (client.get("time", 0) < threshold):
                removeClient(clientList, client)
            else:
                sock.sendto(command["value"].encode('ascii'), (client.get("ip", ""), UDP_PORT))
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
