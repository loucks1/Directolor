/*
  Directolor- Arduino libary for Directolor Blinds using nrf24l01+
  Copyright (c) 2022 Jason Loucks.  All right reserved.

  Project home: https://github.com/sui77/rc-switch/
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#ifndef _Directolor_h
#define _Directolor_h

#include <SPI.h>
#include <RF24.h>
#include <stdint.h>

#define DIRECTOLOR_REMOTE_COUNT 7    // this should match the number of Radios in the RemoteCode const (bottom of this file)
#define DIRECTOLOR_REMOTE_CHANNELS 6 // I tried to use 7 channels and it wouldn't work - looks like we're limited to 6   YMMV
#define DIRECTOLOR_MAX_QUEUED_COMMANDS DIRECTOLOR_REMOTE_COUNT * 2

#define COMMAND_CODE_LENGTH 17
#define DUPLICATE_CODE_LENGTH 18
#define GROUP_CODE_LENGTH 10
#define STORE_FAV_CODE_LENGTH 15

#define MAX_PAYLOAD_SIZE 32 // maximum payload that you can send with the nRF24l01+

#define MESSAGE_SEND_ATTEMPTS 3         // this is the number of times we will generate and send the message (3 seems to work well for me, but feel free to change up or down as needed)
#define MESSAGE_SEND_RETRIES 513        // the number of times to resend the message(seems like numbers > 400 are more reliable - feel free to change as necessary)
#define INTERMESSAGE_SEND_DELAY 512 / 3 // the delay between message sends (if this is too low, the blinds seem to 'miss' messsages)

#define DIRECTOLOR_CAPTURE_FIRST    // with this enabled, it will only show the first message when in capture mode, otherwise, it dumps every message it can - if you want to see full join or remove codes, you'll need to disable this.
#define DIRECTOLOR_DEBUG_SENT_CODES // with this enabled, we'll dump the codes we're sending to the serial port

enum BlindAction
{
    directolor_open = 0x55,
    directolor_close = 0x44,
    directolor_tiltOpen = 0x52,
    directolor_tiltClose = 0x4C,
    directolor_stop = 0x53,
    directolor_toFav = 0x48,
    directolor_setFav = 6, // do this one in a bit...
    directolor_join = 0x01,
    directolor_remove = 0x00,
    directolor_duplicate = 4
};

class Directolor
{

public:
    Directolor(uint16_t _cepin, uint16_t _cspin, uint32_t _spi_speed = RF24_SPI_SPEED); // creates the interface and initializes the radio

    static bool enterRemoteSearchMode(); // puts the code into search mode - when searching, you *must* be using a remote on only one channel and choose one of the regular buttons (open, close, stop, etc)

    void dumpCodes(); // once in remote search mode, if you've found a remote, you can dump to get the codes, then place those found codes into the remoteCodes const at the bottom of this file (don't forget to update the DIRECTOLOR_REMOTE_COUNT #define at the top if you add one)

    bool sendCode(int remoteId, uint8_t channel, BlindAction blindAction); // send a code to a channel (1,2,3,4,5,6) - matches the buttons on the remote (even if you have a 3 button remote, you can still assign and use channels 4-6 with directolor, you just can't access them from the remote)

    bool sendMultiChannelCode(int remoteId, uint8_t channels, BlindAction blindAction); // send a code to multiple channels - channels are a bit mask where the channel on the remote corresponds to 2 ^ (channel - 1).  For example, to send a command to channels 1 & 3, set channels = 5 (2^0 + 2^2)

    void inhibitSend(int durationMS); // maximum of 4 * INTERMESSAGE_SEND_DELAY (if you pass 0 it calls enabledSend()) - use this if, for example, you are also using a 433mhz radio that sends codes as well.  You'd want to inhibitSend on directolor while sending those other codes to avoid shades missing commands

    void enableSend(); // equivalent to inhibitSend(0);

    void processLoop();

private:
    typedef struct CommandItem
    {
        uint8_t *radioCodes;
        uint8_t channels;
        BlindAction blindAction;
        uint8_t resendRemainingCount;
    };

    struct RemoteCode
    {
        uint8_t radioCode[4];
    };

    uint8_t storeFavPrototype[25] = {0x55, 0x55, 0x55, 0X11, 0X11, 0xC0, 0x0F, 0x00, 0x05, 0x2B, 0xFF, 0xFF, 0xBB, 0x0D, 0x86, 0x04, 0x20, 0xBB, 0x0D, 0x63, 0x49, 0x00, 0xC4, 0x10, 0XAA};

    static const uint8_t matchPattern[4]; // this is what we use to find out what the codes for the remote are

    static RF24 radio;
    static bool messageIsSending;
    static bool learningRemote;
    static bool radioValid;
    static RemoteCode remoteCode;
    static unsigned long lastMillis;
    static CommandItem commandItems[DIRECTOLOR_MAX_QUEUED_COMMANDS];
    static short lastCommand;
    static uint16_t _cepin;
    static uint16_t _cspin;
    static uint32_t _spi_speed;
    static bool radioInitialized;

    static bool sendCode(byte *payload, uint8_t payloadSize);
    static bool radioStarted();
    static void checkRadioPayload();
    static bool checkMessageIsSending();
    static void printData(char payload[], int start, int count, char *separator = " ");
    static void enterRemoteCaptureMode();
    static int getRadioCommand(byte *payload, CommandItem commandItem);
    static int getDuplicateRadioCommand(byte *payload, CommandItem commandItem);
    static int getGroupRadioCommand(byte *payload, CommandItem commandItem);

    const RemoteCode remoteCodes[DIRECTOLOR_REMOTE_COUNT] =
        {
            /*
             * Radio:  12 F0 78 09
             */
            {{0x12, 0xF0, 0x78, 0x09}}
            /*
             * Radio:  11 11 B9 7B
             */
            ,
            {{0x11, 0x11, 0xB9, 0x7B}}
            /*
             * Radio:  13 7C BE 09
             */
            ,
            {{0x13, 0x7C, 0xBE, 0x09}}
            /*
             * Radio:  07 B5 CC 83
             */
            ,
            {{0x07, 0xB5, 0xCC, 0x83}}
            /*
             * Radio:  52 C7 75 A9
             */
            ,
            {{0x52, 0xC7, 0x75, 0xA9}}
            /*
             * Radio:  6F F1 EE B7
             */
            ,
            {{0x6F, 0xF1, 0xEE, 0xB7}},
            {{0x56, 0x13, 0x04, 0x67}}}; // this is just random bytes I put it - couldn't get it work when trying to join channel 4, but it did work with channel 1, and then I was able to join channel 4 and remove channel 1 - weird.
};
#endif