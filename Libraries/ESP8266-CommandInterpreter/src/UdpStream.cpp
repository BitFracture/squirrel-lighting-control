
#include "UdpStream.h"

UdpStream::UdpStream(int port) {
	isServer = true;
	this->port = port;
	
	receiveData = String();
	sendData = String();
}

UdpStream::UdpStream(IPAddress address, int port) {
	isServer = false;
	this->port = port;
	this->address = address;
	
	receiveData = String();
	sendData = String();
}

int UdpStream::read() {
	if (!_connected)
		return -1;
	
	//If not receiving a command, see if a packet has arrived
	if (!commandReceiving) {
		if ((commandLength = _connection.parsePacket()) > 0) {
			
			address = _connection.remoteIP();
			int bytesRead = _connection.read(&receiveBuffer[0], commandLength);
			commandLength = bytesRead;
			receiveBuffer[commandLength] = '\0';
			
			//Get the sequence number and data start point
			uint8_t returnCount = 0;
			char* command = &receiveBuffer[0];
			int recSequenceNumber = 0;
			for (int i = 0; i < commandLength; i++) {
				if (receiveBuffer[i] == '\r') returnCount++; else returnCount = 0;
				
				if (returnCount == 2) {
					receiveBuffer[i - 1] = '\0';
					command = &receiveBuffer[i + 1];
					recSequenceNumber = atoi(&receiveBuffer[0]);
					break;
				}
			}
			
			//Negative sequence number means sync packet
			if (recSequenceNumber == -1) {
				//If negative, send blank with sequence is -1, set sequenceNumber to 0
				sequenceNumber = 0;
			} else {
				//If server, must be > sequenceNumber, set the sequence number
				if (isServer) {
					if (recSequenceNumber > sequenceNumber)
						sequenceNumber = recSequenceNumber;
					else
						goto exitReceiveLoop;
				}
				//If client, must equal sequenceNumber
				else {
					if (sequenceNumber != recSequenceNumber)
						goto exitReceiveLoop;
				}
				
				//Our new command is ready
				receiveCounter++;
				commandIndex = 0;
				commandReceiving = true;
				receiveData = String(command);
				commandLength = receiveData.length();
			}
		}
	}
	exitReceiveLoop:
	
	//If we are receiving, fetch a byte
	if (commandReceiving) {
		int toReturn = receiveData[commandIndex++];
		
		if (commandIndex >= commandLength)
			commandReceiving = false;
		
		return toReturn;
	} 
	
	//Return -1 until data a full command exists
	return -1;
}

size_t UdpStream::write(uint8_t u_Data) {
	
	//Write to the command buffer string
	sendData = sendData + (char)u_Data;

	return 1;
}
	
/**
 * Triggers the sending of a UDP packet.
 */
void UdpStream::flush() {
	
	if (!_connected)
		return;
	
	if (isServer) {
		//If server, use existing sequenceNumber
	} else {
		//If client, increment sequenceNumber first
		sequenceNumber++;
	}
	
	_connection.beginPacket(address, port);
	//Send buffered command with sequence number
	_connection.printf("%i\r\r%s", sequenceNumber, sendData.c_str());
	_connection.endPacket();
	sendCounter++;
	sendData = String();
}

uint8_t UdpStream::begin() {
	_connected = _connection.begin(port);
	if (_connected) {
		commandReceiving = false;
		receiveData = String();
		sendData = String();
	}
	
	return _connected;
}

void UdpStream::stop() {
	_connection.stop();
	_connected = false;
}

uint8_t UdpStream::connected() {
	return _connected;
}