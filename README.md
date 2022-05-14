# Directolor
ESP32 + NRF24L01+ library to control Levolor blinds


This is a library known to work with ESP32 and NRF24L01+.  It allows direct control of Levolor blinds by replaying stored remote codes.  The easiest way to use this is to install the library and open the Directolor example.  You can then clone one of the Directolor remotes to a remote you own.  Then you can program your shades to the new cloned remote or using Directolor and control the blinds with either Directolor or your own remote.  

This differs from Wevolor (https://wevolor.com/) in that you DON'T need a Levolor 6 button remote.  

To connect the ESP32 to the NRF24L01+ connect:
rf24 pin    ESP pin
1           ground
2           3.3V Note (10uF cap across ground and 3.3V) (Note:  I haven't needed the cap, but some people have)
3 (CE)      22
4 (CSN)     21
5 (SCK)     18
6 (MOSI)    23
7 (MISO)    19

