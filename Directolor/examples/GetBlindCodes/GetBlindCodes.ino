/**
 * Run this and monitor the serial output.
 * You will need to connect a NRF24L01+ via the SPI on pins 22 & 21 - you'll need more connected.   
 * Follow the NRF24L01+ instructions for connecting.
 * When it runs it will enter search mode.  Choose a single channel on your remote - press the open button
 * You may need to press it a few times to get it found
 * Once you have found it, then just press keys until you've pressed the keys 3 times each.  Do this for each channel as well as join, remove and set favorite.
 * 
 * Then press 'd' to "dump" the codes - this will be in the correct format to add to the Directolor library - don't forget to change the Maximum remotes, if needed.  
 * 
 * Works well on an ESP32 WROOM
*/

#include "Directolor.h"

Directolor directolor(22, 21);

void setup() {

  Serial.begin(115200);
  while (!Serial) {
    // some boards need to wait to ensure access to serial over USB
  }

  delay(500);

  if (! directolor.enterRemoteSearchMode())
  {
    Serial.println("Unable to enter remote capture mode...");
  }
}

void loop() {
  if (Serial.available())
  {
    char c = Serial.read();
    switch (c) {
      case 'd':
        directolor.dumpCodes();
        break;
      case 's':
        directolor.sendCode(6, 1, directolor_stop);
        break;
      case 'o':
        directolor.sendCode(6, 1, directolor_open);
        break;
      case 'c':
        directolor.sendCode(6, 1, directolor_close);
        break;
      case 'j':
        directolor.sendCode(6, 1, directolor_join);
        break;
      case 'r':
        directolor.sendCode(6, 1, directolor_remove);
        break;
    }
  }
} // loop
