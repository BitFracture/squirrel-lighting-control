/**
 * The Flying Squirrels: Squirrel Lighting Controller
 * Purpose: Simplify command handling logic across devices
 * Author:  Erik W. Greif
 * Date:    2017-10-13
 */

#include "CommandInterpreter.h"

CommandInterpreter::CommandInterpreter() {
}

/**
 * Clone registered command information, present a new buffer.
 */
CommandInterpreter::CommandInterpreter(CommandInterpreter& toCopy) {
  for (int i = 0; i < CMD_MAX_COUNT * (CMD_LENGTH + 1); i++)
    cmdNames[i] = toCopy.cmdNames[i];
  for (int i = 0; i < CMD_MAX_COUNT; i++)
    cmdFunctions[i] = toCopy.cmdFunctions[i];
  cmdFunctionDefault = toCopy.cmdFunctionDefault;
  cmdCount = toCopy.cmdCount;
  cmdPointer = 0;
  prefix = toCopy.prefix;
}

/**
 * Executes a registered function by its index in the cmdFunctions list.
 * 
 * @param index  The index of the registered command (returned from register).
 */
void CommandInterpreter::execute(Stream& port, char* command, char* arguments) {
  typedef void func(Stream&, int, const char**);

  //Divide up arguments
  int argc = 0;
  char last = '\0';
  bool inQuotes = false;
  int offset = 0;
  bool escaped = false;
  for (int i = 0; arguments[i + offset] != '\0' && argc < CMD_MAX_ARGS; i++) {

    //Handle escaped characters
    arguments[i] = arguments[i + offset];
    if (!escaped && arguments[i] == '\\') {
      offset++;
      i--;
      escaped = true;
      continue;
    }
    
    if (!inQuotes && arguments[i] == ' ')
      arguments[i] = '\0';
    
    if (arguments[i] == '"' && !escaped) {
      arguments[i] = '\0';
      inQuotes = !inQuotes;
    }
    
    if (last == '\0' && arguments[i] != '\0') {
      cmdArgPointers[argc++] = arguments + i;
    }
    last = arguments[i];
    escaped = false;
  }

  //Find the function and execute
  for (int i = 0; i < cmdCount; i++) {

    if (strcmp(command, &cmdNames[i * (CMD_LENGTH + 1)]) == 0) {

      if (cmdFunctions[i] == NULL)
        break;
      
      func* f = (func*)cmdFunctions[i];
      f(port, argc, (const char**)&cmdArgPointers[0]);
      return;
    }
  }

  if (cmdFunctionDefault == NULL)
    return;

  func* f = (func*)cmdFunctionDefault;
  f(port, argc, (const char**)&cmdArgPointers[0]);
}

/**
 * Registers a new function with a matching command.
 * 
 * @param commandName  String command text max length of CMD_LENGTH.
 * @param commandToRegister  integer no-args function to call for this command.
 * @return  The command index for this command or -1 if failed.
 */
int CommandInterpreter::assign(char* commandName, 
    void (*commandToRegister)(Stream&, int, const char**)) {
  if (cmdCount >= CMD_MAX_COUNT)
    return -1;
  
  cmdFunctions[cmdCount] = (unsigned)commandToRegister;
  int nameOffset = (CMD_LENGTH + 1) * cmdCount;
  for (int i = 0; commandName[i] != '\0' && i < CMD_LENGTH; i++) {;

    cmdNames[i + nameOffset] = commandName[i];
    cmdNames[i + nameOffset + 1] = '\0';
  }
  
  return cmdCount++;
}

/**
 * Assigns a default function to be called when a command is entered that
 * does not resolve to any registered command.
 */
int CommandInterpreter::assignDefault(
    void (*commandToRegister)(Stream&, int, const char**)) {
  
  cmdFunctionDefault = (unsigned)commandToRegister;
}

/**
 * Handles commands if they are present (between % and \n)
 * 
 * @param port  The stream port to read for incomming commands (IP/UART/SoftSerial).
 */
void CommandInterpreter::handle(Stream& port) {
  char* entryPointer = NULL;

  //Receiving commands, may never finish
  while (port.available() > 0) {
    if (cmdPointer >= CMD_BUFFER_SIZE) {
      //Command too long, discard it. Safe option.
      cmdPointer = 0;
      return;  
    }

    //Retrieve value, handle exceptions
    char received = port.read();
    
    if (received == '\r') 
      continue;
    
    if (cmdPointer == 0 && prefix != '\0' && received != prefix)
      continue;

    //Store the char, null terminate and increment
    cmdBuffer[cmdPointer] = received;
    cmdBuffer[++cmdPointer] = '\0';

    //Get a command ready to execute
    if (cmdBuffer[cmdPointer - 1] == '\n') {
      cmdBuffer[cmdPointer - 1] = '\0';
      entryPointer = &cmdBuffer[prefix != '\0'];
      cmdPointer = 0;
    }
  }

  if (entryPointer != NULL) {
    process(port, entryPointer);
  }
}

/**
 * Handle UDP packets as commands.
 * Note: No command prefix supported at this time. 
 */
void CommandInterpreter::handleUdp(WiFiUDP& port) {
  
  int packetSize = port.parsePacket();
  if (!packetSize)
	  return;
  
  char* bufferPointer = &cmdBuffer[0];
  int len = port.read(bufferPointer, CMD_BUFFER_SIZE);
  
  //Check the command prefix
  if (prefix != '\0') {
	if (bufferPointer[0] == prefix)
		bufferPointer++;
	else
		return;
  }
  
  //Null terminate and find new line for command end
  cmdBuffer[len] = 0;
  for (int i = 0; cmdBuffer[i] != 0; i++) {
	if (cmdBuffer[i] == '\n') {
	  cmdBuffer[i] = '\0';
	  break;
	}
  }
  
  stringStream.clear();
  process(stringStream, bufferPointer);
  
  //If the user made any stream response, send that response in a packet
  String response = stringStream.getString();
  if (response.length() > 0) {
	port.beginPacket(port.remoteIP(), port.remotePort());
	port.print(response);
	port.endPacket();
  }
}

/**
 * Take a command buffer pointer, chop into a command an an argument string.
 * The execute function will split the arguments apart. (FOR NOW)
 * TO-do: Move the argument splitting to this function.
 */
void CommandInterpreter::process(Stream& port, char* entryPointer) {
  
  //Generate the command pointer
  char* command = entryPointer;
  char* bufPtr = entryPointer;
  for (; *bufPtr != ' ' && *bufPtr != '\0'; bufPtr++);
  *bufPtr = '\0';
  
  //Find the args
  for (bufPtr++; *bufPtr == ' ' && *bufPtr != '\0'; bufPtr++);
  char* arguments = bufPtr;

  //Invoke the function
  execute(port, command, arguments);
}

/**
 * 
 */
void CommandInterpreter::setPrefix(char newPrefix) {
  prefix = newPrefix;
}
 
