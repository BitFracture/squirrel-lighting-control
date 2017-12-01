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
  
  void execute(Stream&, char*, char*);
  void process(Stream&, char*);
  
public:
  CommandInterpreter();
  CommandInterpreter(CommandInterpreter&);
  int assign(char*, void (*)(Stream&, int, const char**));
  int assignDefault(void (*)(Stream&, int, const char**));
  void setPrefix(char);
  void handle(Stream&);
  void handleUdp(WiFiUDP&);
};
