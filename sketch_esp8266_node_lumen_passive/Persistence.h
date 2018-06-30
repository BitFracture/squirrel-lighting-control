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

//Default static arrays
static const char*   DEFAULT_SSID    = "network-ssid\0";
static const char*   DEFAULT_PASS    = "network-pass\0";
static const char*   DEFAULT_ZONE    = "\0";
static const char*   HEADER          = "BFSQ";
static const float   DEFAULT_HUES[]  = {0.0f, 60.0f, 120.0f, 180.0f, 240.0f, 300.0f};
static const uint8_t DEFAULT_WARM[]  = {0, 0, 0, 0, 255, 0};
static const uint8_t DEFAULT_COOL[]  = {0, 0, 0, 255, 0, 0};
static const uint8_t DEFAULT_COLOR[] = {0, 0, 0, 255, 255, 0};

class Persistence {
public:
  static const uint16_t PERSISTENCE_VERSION = 2;
  static const int      HEADER_LENGTH = 4;
  
private:
  //Data serialization structs. ORDER MATTERS.
  //Do not move or remove the first two attributes!
  struct PackedDataHeader {
    char header[4];
    uint16_t version;
  };
  struct PackedDataV2 {
    char header[4];
    uint16_t version;
    uint8_t cycles;
    char ssid[33];
    char pass[33];
    char name[33];
    char zone[33];
    char bootMode;
    uint8_t defaultColor[6];
    uint8_t pureCool[6];
    uint8_t pureWarm[6];
    float hues[6];
  };
  struct PackedDataV1 {
    char header[4];
    uint16_t version;
    uint8_t cycles;
    char ssid[33];
    char pass[33];
    char name[33];
    char zone[33];
    uint8_t defaultColor[6];
    float hues[6];
    uint8_t pureCool[6];
    uint8_t pureWarm[6];
  };
  //Typedef the latest version of persistence data so we don't 
  //have to constantly refactor things
  typedef struct PackedDataV2 PackedData;
  
  PackedData packedData;
  bool dirty;
  char* id = "000000";

  /**
   * Uses entropy of the MAC address and current micros to generate a 
   * pseudo-random seed
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
   * Performs a series of validations and enforcements. Does NOT save changes 
   * to persistence.The controlling application must call dump to  clear the 
   * dirty bit.
   */
  Persistence(PackedData packed, bool isDirty) : packedData(packed), dirty(isDirty) {
    //Basic requirements (forced delimiters)
    packedData.ssid[32] = 0;
    packedData.pass[32] = 0;
    packedData.name[32] = 0;
    packedData.zone[32] = 0;
    
    randomSeed(generateSeed());

    //Unique ID for this session
    for (int i = 0; i < 6; i++)
      id[i] = '0' + random(0, 10);

    if (packedData.version != PERSISTENCE_VERSION)
      loadDefaults();
  }
  
  template<typename T>
  static T persistenceToStruct() {
    int serialSize = sizeof(T);
    T packed;
    
    for (int i = 0; i < serialSize; i++)
      *(((byte*)(&packed)) + i) = EEPROM.read(i);

    return packed;
  }

  /**
   * Migration method for persistence v1 to v2.
   * 
   * Features:
   *  - Adds boot mode for OTA updating
   */
  static PackedDataV2 migrateV1V2(PackedDataV1 oldData) {
    Serial.println("Migrating to persistence V2");
    PackedDataV2 newData;
    newData.version = 2;
    newData.cycles = oldData.cycles;
    memcpy(&newData.header[0], &oldData.header[0], 4);
    memcpy(&newData.ssid[0], &oldData.ssid[0], 33);
    memcpy(&newData.name[0], &oldData.name[0], 33);
    memcpy(&newData.pass[0], &oldData.pass[0], 33);
    memcpy(&newData.zone[0], &oldData.zone[0], 33);
    memcpy(&newData.defaultColor[0], &oldData.defaultColor[0], 6);
    memcpy(&newData.pureCool[0], &oldData.pureCool[0], 6);
    memcpy(&newData.pureWarm[0], &oldData.pureWarm[0], 6);
    memcpy(&newData.hues[0], &oldData.hues[0], sizeof(float) * 6);
    
    newData.bootMode = 0;
    return newData;
  }

  void writeStringAttribute(char* fromStr, char* toStr, int maxLength) {
    int len = strlen(fromStr);
    if (len > maxLength) len = maxLength;
    
    if (strlen(toStr) != len || memcmp(toStr, fromStr, len) != 0) {
      memcpy(toStr, fromStr, len);
      toStr[len] = 0;
      dirty = true;
    }
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
    PackedDataHeader packedHead = persistenceToStruct<PackedDataHeader>();
    uint16_t version = packedHead.version;

    //Make sure data format header matches exactly
    for (int i = 0; i < HEADER_LENGTH; i++)
      if (packedHead.header[i] != (char)HEADER[i]) version = 0;

    PackedData packed;
    bool isDirty = true;
    switch (version) {
      case 2: 
        packed = persistenceToStruct<PackedData>();
        isDirty = false;
        break;
      case 1: 
        packed = migrateV1V2(persistenceToStruct<PackedDataV1>());
        isDirty = false;
        break;
      default:
        //This is an invalid version and will trigger defaults to load
        packed.version = 0;
        break;
    }
    return Persistence(packed, isDirty);
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

  void dumpIfDirty() {
    if (dirty)
      dump();
  }
  
  boolean getIsDirty() {
    return dirty;
  }

  void loadDefaults() {
    Serial.println("Loading persistence defaults");
    
    packedData.version = PERSISTENCE_VERSION;
    for (int i = 0; i < HEADER_LENGTH; i++)
      packedData.header[i] = (char)HEADER[i];
    packedData.cycles = 0;
    setSsid((char*)DEFAULT_SSID);
    setPass((char*)DEFAULT_PASS);
    setZone((char*)DEFAULT_ZONE);
    memset(&packedData.name[0], 0, 33);
    for (int i = 0; i < 6; i++)
      packedData.name[i] = 'a' + random(0, 26);
    for (int i = 0; i < 6; i++) {
      packedData.defaultColor[i] = DEFAULT_COLOR[i];
      packedData.pureCool[i]     = DEFAULT_COOL[i];
      packedData.pureWarm[i]     = DEFAULT_WARM[i];
      packedData.hues[i]         = DEFAULT_HUES[i];
    }
    packedData.bootMode = 0;
    dirty = true;
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

  const char* getZone() {
    return (const char*)&packedData.zone[0];
  }

  const char* getTransientId() {
    return (const char*)id;
  }
  
  uint8_t incrementAndGetCycles() {
    packedData.cycles++;
    dirty = true;
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
    dirty = packedData.cycles != 0;
    packedData.cycles = 0;
  }
  
  void setSsid(char* newSsid) {
    writeStringAttribute(newSsid, &packedData.ssid[0], 32);
  }

  void setPass(char* newPass) {
    writeStringAttribute(newPass, &packedData.pass[0], 32);
  }

  void setName(char* newName) {
    writeStringAttribute(newName, &packedData.name[0], 32);
  }

  void setZone(char* newZone) {
    writeStringAttribute(newZone, &packedData.zone[0], 32);
  }
};

#endif

