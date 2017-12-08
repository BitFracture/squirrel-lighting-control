import serial
import time

def millis():
  return int(round(time.time() * 1000))

ser = serial.Serial('COM7', baudrate=9600, bytesize=8, stopbits=1, timeout=None)


class Average:
  def __init__(self, len):
    self.maxLen = len
    self.count = 0
    self.values = [0]
    self.maxLen = 30
  
  def add(self, i):
    self.count += i
    self.values.insert(0, i)
    if len(self.values) > self.maxLen:
      self.count -= self.values.pop();
  
  def average(self):
    return self.count // self.maxLen;

  def length(self):
    return len(self.values)

avg = Average(8);
avgMax = Average(14);

lastLoudTime = 0
readNum = ""
iLast = 0
iMax = 0

while True:
  c = ser.read(1).decode("ASCII")
  
  if (c == ' '):
    if (len(readNum) > 0 and readNum[0] != '*'):
      iCurent = int(readNum)
      
      if iCurent <= iLast:
        if iMax > 10:
          if (millis() - lastLoudTime) < 500:
            print("-" * iMax)
            lastLoudTime = 0
          else:
            lastLoudTime = millis()
        
        iMax = -1
      else:
        iMax = iCurent
      
      print("*" * iCurent + str(iCurent))
      
      iLast = iCurent
      pass
      
    else:
      print(readNum)
      pass
    
    readNum = ""
  else:
    readNum += c