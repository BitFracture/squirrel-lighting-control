/** 
 * The Flying Squirrels: Squirrel Lighting Controller
 * Hardware: ESP8266EX
 * Purpose:  Define HTML web string constants. These will be streamed together.
 * Author:   Erik W. Greif
 * Date:     2018-06-22
 */

#ifndef __WEB_STRINGS
#define __WEB_STRINGS

const char* WEB_HEADER = 
  "<!DOCTYPE html>\n<html>"
  "<head>"
    "<style>"
      "div.container {max-width: 400px; margin-left: auto; "
        "margin-right: auto; background-color: #FFFFFF; padding: 24px; "
        "border-radius: 16px; }"
      "body {background-color: #555555;}"
      "input[type=text] {border: .1em solid #000000;}"
      "input[type=password] {border: .1em solid #000000;}"
      "p, label, input {font-size: 20px !important;}"
      "h1 {font-size: 48px;}"
      "h2 {font-size: 32px;}"
    "</style>"
    "<meta name=\"viewport\" content=\"width=500, "
    "target-densitydpi=device-dpi\">"
  "</head>"
  "<body>"
    "<div class=\"container\">";
        
const char* WEB_FOOTER = 
  "</div>"
  "</body></html>";

const char* WEB_BODY_HOME = 
  "<h1>Squirrel</h1><h2>Wi-Fi Lighting Control System</h2>"
  "<p>Welcome to your Squirrel light bulb! The bulb you are connected to "
  "is in setup mode, and is cycling through various temperatures or "
  "colors. If you cannot see this, your bulb may "
  "have been calibrated incorrectly and should be factory reset. </p>"
  "<p>If you want to begin using your bulb, you should start "
  "by connecting it to your wireless network.</p>"
  "<p>What would you like to do?</p>"
  "<form method=\"get\" action=\"/connect\">"
    "<input type=\"submit\" value=\"Connect to a network\"></input>"
  "</form><br/>"
  "<form method=\"get\" action=\"/reset\">"
    "<input type=\"submit\" value=\"Factory reset\"></input>"
  "</form><br/>"
  "<form method=\"get\" action=\"/exit\">"
    "<input type=\"submit\" value=\"Exit setup\"></input>"
  "</form>";

const char* WEB_BODY_RESET = 
  "<h1>Squirrel</h1><h2>Factory Reset</h2>"
  "<p>Selecting &quot;Reset now&quot; will erase any customization to this "
  "bulb and will establish the factory default settings. After rebooting, the "
  "bulb will require you to enter setup mode again to connect it to your "
  "network.</p>"
  "<form method=\"post\">"
    "<input type=\"submit\" value=\"Reset now\" />"
  "</form><br/>"
  "<form method=\"get\" action=\"/home\">"
    "<input type=\"submit\" value=\"Back\" />"
  "</form>";

const char* WEB_BODY_RESET_COMPLETE = 
  "<h1>Squirrel</h1><h2>Reset complete</h2>"
  "<p>Your Squirrel is restarting with factory settings. "
  "If you wish to use this bulb again, enter setup mode again and "
  "connect the bulb to your network.</p>";

const char* WEB_BODY_CONNECT = 
  "<h1>Squirrel</h1><h2>Connect To Your Network</h2>"
  "<p>In order to use this Squirrel, "
  "it must connect to your local wireless network. This is probably the "
  "same network you use every day.<br/>Enter that information here "
  "and submit the form.</p>"
  "<form method=\"post\">"
    "<label>Wi-Fi Name (SSID)</label><br/>"
    "<input name=\"ssid\" type=\"text\" value=\"\"></input><br/>"
    "<label>Wi-Fi Password</label><br/>"
    "<input name=\"pass\" type=\"password\" value=\"\"></input><br/><br/>"
    "<input type=\"submit\" value=\"Save\"></input>"
  "</form><br/>"
  "<form method=\"get\" action=\"/home\">"
    "<input type=\"submit\" value=\"Back/discard\"></input>"
  "</form>";

const char* WEB_BODY_CONNECT_COMPLETE = 
  "<h1>Squirrel</h1><h2>Thank you</h2>"
  "<p>Your Squirrel is restarting and will try to connect "
  "to your network. If the bulb does not connect to your controller, try "
  "these connection steps again, as this may be a sign that you have typed "
  "your network settings incorrectly. If your network is an AC "
  "(5GHz band) network, you must enable the B, G, or N wireless network "
  "to connect this device.</p>";

const char* WEB_BODY_CONNECT_FAILED = 
  "<h1>Squirrel</h1><h2>Something is wrong...</h2>"
  "<p>Your network name and password may not be more than 32 characters. "
  "Your network name may not be empty. These are the same credentials you use "
  "to connect your phone or personal computer to your network.</p>"
  "<form method=\"get\" action=\"/connect\">"
    "<input type=\"submit\" value=\"Try again\"></input>"
  "</form>";

const char* WEB_BODY_EXIT = 
  "<h1>Squirrel</h1>"
  "<p>Your bulb is rebooting for normal operation.</p>";

#endif

