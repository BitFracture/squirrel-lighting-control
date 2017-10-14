/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Purpose: Simplify command handling logic across devices
 * Author:  Erik W. Greif
 * Date:    2017-10-13
 */

#pragma once

#include <ESP8266HTTPClient.h>

class CommandInterpreter {

private:
	//Number of commands possible to be registered (preallocated)
	const static int CMD_MAX_COUNT = 32;
	//Number of chars per command
	const static int CMD_LENGTH = 16;
	//Size of receive buffer (max total command size plus args and LF)
	const static int CMD_BUFFER_SIZE = 255;

	//Command receive buffer
	char cmdBuffer[CMD_BUFFER_SIZE + 1];
	//Store each command in one buffer, equally spaced
	char cmdNames[CMD_MAX_COUNT * (CMD_LENGTH + 1)];
	//Store each command function (int, no-args!)
	unsigned cmdFunctions[CMD_MAX_COUNT];
	unsigned cmdFunctionDefault = 0;
	//Store number of registered commands
	int cmdCount = 0;
	//If command is being read, this will be non-0
	int cmdPointer = 0;
  //Command prefix character
  char prefix = '\0';
	
public:
	CommandInterpreter();
  CommandInterpreter(CommandInterpreter&);
	void execute(Stream&, char*, char*);
	int assign(char*, void (*)(Stream&));
	int assignDefault(void (*)(Stream&));
  void setPrefix(char);
	void handle(Stream&);
};

