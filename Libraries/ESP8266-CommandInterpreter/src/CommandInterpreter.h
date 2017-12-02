/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Purpose: Simplify command handling logic across devices
 * Author:  Erik W. Greif
 * Date:    2017-10-13
 */

#pragma once

#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <Stream.h>

class CommandInterpreter {

private:

  //Special class to pass when stream commands cannot reply (UDP)
  class NullStream : public Stream {
  public: 
    NullStream()     { return;    }
    int  available() { return 0;  }
    void flush()     { return;    }
    int  peek()      { return -1; }
    int  read()      { return -1; }
    size_t write(uint8_t u_Data){ return 0x01; }
  };
  
  //Special class to capture stream as String
  class StringStream : public Stream {
  public:
    StringStream()   { return;    }
    int  available() { return 0;  }
    void flush()     { return;    }
    int  peek()      { return -1; }
    int  read()      { return -1; }
    size_t write(uint8_t u_Data) {
		
		stringData = stringData + (char)u_Data;
		return 0x01;
	}
	String getString() { return stringData; }
	String clear() { stringData = String(""); }

  private:
	String stringData;
  };
  
  //Special class to buffer commands before sending them to another stream
  //    A new line character is used to send. Each send is counted. 
  class CommandBufferStream : public Stream {
  public:
    CommandBufferStream(Stream& sourceStream, int* sendCounter = 0, int* receiveCounter = 0) {
		this->sourceStream = &sourceStream;
		stringReceiveData = String();
		stringData = String();
		sendCount = sendCounter != 0 ? sendCounter : &internalSendCounter;
		receiveCount = receiveCounter != 0 ? receiveCounter : &internalReceiveCounter;
	}
    int  available() { return sourceStream->available(); }
    void flush()     { sourceStream->flush(); }
    int peek()      { return sourceStream->peek(); }
	
    int read() {
		int realData = sourceStream->read();
		if (realData <= 0) return realData;
		
		uint8_t data = (uint8_t)(char)realData;
		stringReceiveData += (char)data;
		
		//Send echo now?
		if (data == (uint8_t)'\n') {
			if (sequenceNumbersEnabled)
				sourceStream->printf("[%i] %s", *receiveCount, stringReceiveData.c_str()); //TODO: real receive counter
			else
				sourceStream->print(stringReceiveData);
			stringReceiveData = "";
			(*receiveCount)++;
		}
		
		return realData;
	}
    size_t write(uint8_t u_Data) {
		
		stringData = stringData + (char)u_Data;
		
		//Send buffer now?
		if (u_Data == (uint8_t)'\n') {
			if (sequenceNumbersEnabled)
				sourceStream->printf("[%i] %s", *sendCount, stringData.c_str());
			else
				sourceStream->print(stringData);
			stringData = "";
			(*sendCount)++;
		}
		
		return 0x01;
	}
	String getString() { return stringData; }
	String getReceiveString() { return stringReceiveData; }
	void enableSequenceNumbers(bool enable) { this->sequenceNumbersEnabled = enable; }

  private:
	String stringReceiveData;
	String stringData;
	Stream* sourceStream;
	int internalReceiveCounter;
	int internalSendCounter;
	int* sendCount;
	int* receiveCount;
	bool sequenceNumbersEnabled = false;
  };
  
  //Number of commands possible to be registered (preallocated)
  const static int CMD_MAX_COUNT = 32;
  //Number of chars per command
  const static int CMD_LENGTH = 16;
  //Size of receive buffer (max total command size plus args and LF)
  const static int CMD_BUFFER_SIZE = 255;
  //Number of arguments possible
  const static int CMD_MAX_ARGS = 16;
  //A pre-constructed null stream to send to UDP requests
  NullStream nullStream;
  StringStream stringStream;

  //Command receive buffer
  char cmdBuffer[CMD_BUFFER_SIZE + 1];
  char cmdNames[CMD_MAX_COUNT * (CMD_LENGTH + 1)];
  char* cmdArgPointers[CMD_MAX_ARGS];
  //Store each command function (int, no-args!)
  unsigned cmdFunctions[CMD_MAX_COUNT];
  unsigned cmdFunctionDefault = 0;
  //Store number of registered commands
  int cmdCount = 0;
  //If command is being read, this will be non-0
  int cmdPointer = 0;
  //Command prefix character
  char prefix = '\0';
  
  //Store command counters
  int receiveCount = 0;
  int sendCount = 0;
  
  void execute(Stream&, char*, char*);
  void process(Stream&, char*);
  bool echoEnabled = false;
  bool sequenceNumbersEnabled = false;
  
public:
  CommandInterpreter();
  CommandInterpreter(CommandInterpreter&);
  int assign(char*, void (*)(Stream&, int, const char**));
  int assignDefault(void (*)(Stream&, int, const char**));
  void setPrefix(char);
  void handle(Stream&);
  void handleUdp(WiFiUDP&);
  void enableEcho(bool);
  void enableSequenceNumbers(bool);
  
  int getSendCount();
  int getReceiveCount();
};
