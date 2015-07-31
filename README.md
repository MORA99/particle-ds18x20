Ported AVR code for interacting with DS18B20 sensors.

Supports multiple sensors on one pin.

Standard hardware configuration, 4.7k ohm resistor from data to VCC line.

Note that this is C code, not CPP, so functions etc are not enclosed in their own name space.
The functions are named crc8 for the CRC calculations, ow_ for onewire functions, and DS18X20_ for temperature specific functions.
