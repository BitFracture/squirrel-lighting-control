# Squirrel Lighting Controller

## Brief Description

A network of ESP01 modules controls overhead lighting brightness and temperature based on input from exterior lighting, interior motion detection, pressure from a bed or chair, and audio input. Manual overrides of these values are also possible.

## Get Started

1. To edit these sketches, you must first install Arduino IDE.
2. Add the ESP8266 board option to your IDE (http://esp8266.github.io/Arduino/versions/2.2.0/doc/installing.html).
3. Install the libraries for ESP8266 WiFi, in `External Libraries` folder.
    - On Windows, copy to `\%USERPROFILE%\Documents\Arduino\libraries\`
	- On MacOS, copy to `~/Documents/Arduino/libraries/`
4. Install custom libraries, in `Libraries` folder. 
5. Choose the Generic ESP8266 for board type.
6. Compile a blank sketch to see if it succeeds.

Code not compiling in a production branch? Double check that you have the most 
recent versions of the custom libraries, as these may update frequently. You 
must manually install these. 

Please note: Never store passwords in the repository unless they are used 
for authentication between bodies of code within this repository. External
passwords present a security risk when placed in a public repository. 

## Flashing Firmware

1. Have the compiled sketch in Arduino IDE.
2. Hold GPIO-0 to ground (or use button with pull-up).
3. Momentarily ground RESET (or use button with pull-up).
4. Release GPIO-0 back to high state.
5. Select Upload from Arduino IDE and wait for completion.

## Custom Library Code

To ease in the simplicity of custom library code, libraries may be developed inside a sketch folder. Once these libraries are trusted and usable, they should be given a descriptor and installed like other Arduino libraries. Eventually, we do not want code copied between sketch folders, but during library development that will be ok. 

## Checklist

[=] Build reusable command interpreter library  
[=] Make command methods take an argument array  
[ ] Define server node command logic  
[=] Define communication between nodes  
[ ] ...
