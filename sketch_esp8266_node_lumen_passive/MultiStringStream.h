/**
 * Enables streaming data from a set of strings. A double array of pointers to strings
 * will be traversed until null terminated.
 * 
 * @author  Erik W. Greif
 * @since   2018-06-21
 */

#ifndef __MULTI_STRING_STREAM
#define __MULTI_STRING_STREAM

class MultiStringStream: public Stream {
public:
  MultiStringStream(const void** nestedDataArray) {
    outerPointer = (uint8_t**)nestedDataArray;
    innerPointer = &knownNull;
    originalSize = available();
  }
  virtual int available() {
    uint8_t** tempOuterPointer = outerPointer;
    int length = strlen((const char*)innerPointer);
    for (; *tempOuterPointer != NULL; tempOuterPointer++)
      length += strlen((const char*)*tempOuterPointer);
    return length;
  }
  virtual int read() {
    while (*innerPointer == NULL) {
      if (*outerPointer == NULL)
        return -1;
      
      innerPointer = *outerPointer;
      outerPointer++;
    }
    int value = (int)*innerPointer;
    innerPointer++;
    return value;
  }
  virtual int read(void* buffer, int quantityExpected) {
    int response = 0;
    int quantity = 0;
    while ((quantityExpected - quantity) > 0) {
      response = read();
      if (response < 0)
        break;
      ((uint8_t*)buffer)[quantity] = (uint8_t)response;
      quantity++;
    }
    return quantity;
  }
  virtual int peek() {
    uint8_t** savedOuterPointer = outerPointer;
    uint8_t* savedInnerPointer = innerPointer;
    int value = read();
    outerPointer = savedOuterPointer;
    innerPointer = savedInnerPointer;
    return value;
  }
  virtual void flush() {}
  virtual size_t write(uint8_t) {return 0;}
  virtual int size() {return originalSize;}
  virtual const char* name() {return "";}
private:
  uint8_t** outerPointer;
  uint8_t* innerPointer;
  uint8_t knownNull = NULL;
  long originalSize;
};

#endif

