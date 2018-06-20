/** 
 * The Flying Squirrels: Squirrel Lighting Controller
 * Hardware: ESP8266EX
 * Purpose:  Define persistence handling
 * Author:   Erik W. Greif
 * Date:     2018-06-19
 */

#ifndef __PERSISTENCE
#define __PERSISTENCE

#include <ESP8266WiFi.h>
#include <EEPROM.h>

class Persistence {
private:
  struct PackedData {
    uint8_t ver;
    uint16_t port;
    uint8_t cycles;
    char ssid[33];
    char pass[33];
    char name[33];
    uint8_t defaultColor[6];
    float hues[6];
    uint8_t pureWhite[6];
    uint8_t pureWarm[6];
  };
  PackedData packedData;
  bool dirty;
  const uint16_t DEFAULT_PORT = 23;
  const uint8_t PERSISTENCE_VERSION = 1;
  const char* DEFAULT_SSID = "network-ssid\0";
  const char* DEFAULT_PASS = "network-pass\0";
  char* id = "0000";

  /**
   * Uses entropy of the MAC address and current micros to generate a pseudo random seed
   */
  int generateSeed() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    return (int)mac[0] + (int)mac[1] + (int)mac[2] 
         + (int)mac[3] + (int)mac[4] + (int)mac[5] + micros();
  }

  /**
   * Returns true if entire string is in the ASCII range
   */
  bool isAsciiString(char* someString) {
    for (int i = 0; someString[i] != 0; i++)
      if (!isAscii(someString[i]))
        return false;
    return true;
  }
  
  /**
   * Performs a series of validations and loads defaults if anything is out of place.
   * Does NOT save the new defaults. The controlling application must call dump to 
   * clear the dirty bit.
   */
  Persistence(PackedData packed) : packedData(packed), dirty(false) {
    //Basic requirements (forced delimiters)
    packedData.ssid[32] = 0;
    packedData.pass[32] = 0;
    packedData.name[32] = 0;
    
    randomSeed(generateSeed());

    //Check validity, load defaults if not valid
    if (!isAsciiString(&packedData.ssid[0]) || strlen(&packedData.ssid[0]) <= 0) {
      dirty = true;
      memset(&packedData.ssid[0], 0, 33);
      memcpy(&packedData.ssid[0], DEFAULT_SSID, strlen(DEFAULT_SSID));
    }
    if (!isAsciiString(&packedData.pass[0])) {
      dirty = true;
      memset(&packedData.pass[0], 0, 33);
      memcpy(&packedData.pass[0], DEFAULT_PASS, strlen(DEFAULT_PASS));
    }
    if (!isAsciiString(&packedData.name[0]) || strlen(&packedData.name[0]) <= 0) {
      dirty = true;
      memset(&packedData.name[0], 0, 33);
      for (int i = 0; i < 4; i++)
        packedData.name[i] = 'a' + random(0, 26);
    }
    if (packedData.ver != PERSISTENCE_VERSION) {
      dirty = true;
      packedData.ver = PERSISTENCE_VERSION;
    }
    if (packedData.port <= 0) {
      packedData.port = DEFAULT_PORT;
      dirty = true;
    }

    //Unique ID for this session
    for (int i = 0; i < 4; i++)
      id[i] = 'A' + random(0, 26);
  }
public:
  Persistence() : packedData(PackedData()), dirty(false) {}
  Persistence operator=(Persistence toCopy) {
    this->packedData = toCopy.packedData;
    this->dirty = toCopy.dirty;
    return *this;
  }
  /**
   * Constructs a Persistence object from the data stored in EEPROM or Flash.
   */
  static Persistence load() {
    int serialSize = sizeof(PackedData);
    PackedData packed;
    
    for (int i = 0; i < serialSize; i++)
      *(((byte*)(&packed)) + i) = EEPROM.read(i);

    return Persistence(packed);
  }
  static void init() {
    EEPROM.begin(512);
  }
  
  void dump() {
    int serialSize = sizeof(PackedData);
    
    for (int i = 0; i < serialSize; i++)
      EEPROM.write(i, *(((byte*)(&packedData)) + i));

    EEPROM.commit();
    dirty = false;
  }
  
  boolean getIsDirty() {
    return dirty;
  }
  
  const char* getSsid() {
    return (const char*)&packedData.ssid[0];
  }

  const char* getPass() {
    return (const char*)&packedData.pass[0];
  }

  const char* getName() {
    return (const char*)&packedData.name[0];
  }

  uint16_t getPort() {
    return packedData.port;
  }

  const char* getTransientId() {
    return (const char*)id;
  }
  
  uint8_t incrementAndGetCycles() {
    packedData.cycles++;
    return packedData.cycles;
  }
  
  const float* getHues() {
    return (const float*)&packedData.hues[0];
  }

  void setHues(float* newHues) {
    for (int i = 0; i < 6; i++) {
      if (packedData.hues[i] != newHues[i]) {
        packedData.hues[i] = newHues[i];
        dirty = true;
      }
    }
  }

  void resetCycles() {
    packedData.cycles = 0;
  }
  
  void setSsid(char* newSsid) {
    int len = strlen(newSsid);
    if (len > 32) len = 32;
    memcpy(&packedData.ssid[0], newSsid, len);
    packedData.ssid[len] = 0;
    dirty = true;
  }

  void setPass(char* newPass) {
    int len = strlen(newPass);
    if (len > 32) len = 32;
    memcpy(&packedData.pass[0], newPass, len);
    packedData.pass[len] = 0;
    dirty = true;
  }

  void setName(char* newName) {
    int len = strlen(newName);
    if (len > 32) len = 32;
    memcpy(&packedData.name[0], newName, len);
    packedData.name[len] = 0;
    dirty = true;
  }

  void setPort(uint16_t newPort) {
    if (newPort <= 0)
      newPort = DEFAULT_PORT;
    packedData.port = newPort;
    dirty = true;
  }
};

#endif

