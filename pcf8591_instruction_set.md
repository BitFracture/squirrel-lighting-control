
# Defining PCF8591 Instructions

## 1. Address Byte

This is always 0x9X. The lower nibble is 
configured by the device's 3 address 
input pins, and LSB represents read/write.

## 2. Output

This mode is presumed if a write address
is presented.

### A. Control Byte

````0[O][AP]0[I][CN]````

````
O  = Output enable
AP = Analog input programming
	00 = Four, single-ended inputs
	01 = Three differential inputs, ref is 3
	10 = Both
	11 = Two independent diff inputs
```

### B. DAC Register

If the output is enabled, this data byte 
is the output step (0-255) * VDD.

## 3. Input

The input data bytes are read back 
successively after a read address is 
presented. 
