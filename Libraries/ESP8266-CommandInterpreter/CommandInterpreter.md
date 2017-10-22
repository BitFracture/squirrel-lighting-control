
# Command Interpreter for Arduino/ESP8266

This library interprets incoming commands in a stream, and can call associated functions for handling those commands. Arguments are parsed and handed to the function similar to the way a modern operating system does. 

## Associating Functions and Commands

First, create a command interpreter instance. 

```
#include <CommandInterpreter.h>

CommandInterpreter cmdInterpreter;
```

Next, assign one or multiple commands to functions. When the stream sends the command "help" the function commandHelp(...) will be invoked. Note the arguments required for these functions, as they cannot change. The Stream reference here is the stream that invoked the command, so you may reply. `argc` and `argv` are standard for C and C++ main functions, except that the command is not included as an argument. In other words, 0 arguments means argc will be 0, not 1. 

```
void commandHelp(Stream& source, int argc, const char** argv) {
    source.println("Command Help");
    source.println("");
    source.println(" help = View this prompt");
    source.println("");
}

void setup() {
    //Point command handler at the commandHelp function
    cmdInterpreter.assign("help", commandHelp);
}
```

Have a stream you can listen on. For this example, we will use `Serial`, but you may use TCP, UDP, or any other stream. To capture command activity on stream, pass the stream into the handle() function in your loop. 

```
void loop() {
    cmdInterpreter.handle(Serial);
}
```

That's it! You're ready to go. 

## Precautions

Only handle one stream per instance of `CommandInterpreter`. This is because the buffered read from the stream is non-blocking, and reading two streams can mix incoming data in the buffer. 

To get around this, use the copy constructor to create a new instance of `CommandInterpreter` after you have assigned your commands. The assignments will copy over, and the new object will have its own buffers. 

## Useful Functions

### Set Command Prefix

Using this function, you can designate that commands are not to be handled unless they begin with a special character. 

```setPrefix(char prefix)```

### Assign A Default Command

If you want to handle the case where a command is received that doesn't exist, associate a function for the default case. 

```assignDefault(someDefaultFunction)```
