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

#include "Directolor.h"
#include <SPI.h>
#include "printf.h"
#include "RF24.h"

RF24 Directolor::radio;
QueueHandle_t Directolor::queue;
bool Directolor::messageIsSending = false;
bool Directolor::learningRemote = false;
bool Directolor::radioValid = false;
Directolor::RemoteCode Directolor::remoteCode;
long Directolor::lastMillis = 0;

constexpr uint8_t Directolor::matchPattern[4] = {0xC0, 0X11, 0X00, 0X05};                       // this is what we use to find out what the codes for the remote are
const uint8_t Directolor::commandDynamicBytesRead[COMMAND_CODE_UNIQUE_BYTES] = {3, 10, 18, 19}; // 6&7 static id, 16 == command
const uint8_t Directolor::duplicateDynamicBytesRead[DUPLICATE_CODE_UNIQUE_BYTES] = {3, 6, 7, 8, 9, 10, 11, 12, 13, 19, 20};
const uint8_t Directolor::groupDynamicBytesRead[GROUP_CODE_UNIQUE_BYTES] = {3, 11, 12};
const uint8_t Directolor::storeFavDynamicBytesRead[STORE_FAV_UNIQUE_BYTES] = {3, 10, 16, 17};

Directolor::Directolor(uint16_t _cepin, uint16_t _cspin, uint32_t _spi_speed)
{
  radio = RF24(_cepin, _cspin, _spi_speed);
  queue = xQueueCreate(MESSAGE_QUEUE_LENGTH, sizeof(CommandItem));
  messageIsSending = false;

  xTaskCreatePinnedToCore(
      sendCodeTask,   /* Function to implement the task */
      "sendCodeTask", /* Name of the task */
      10000,          /* Stack size in words */
      NULL,           /* Task input parameter */
      2,              /* Priority of the task */
      NULL,           /* Task handle. */
      0);             /* Core where the task should run */
}

/**
 * Sets the protocol to send.
 */
bool Directolor::radioStarted()
{
  if (!radioValid)
  {
    Serial.println("Attempting to start radio");
    radioValid = radio.begin();
    if (radioValid)
    {
      radio.setAutoAck(false);               // auto-ack has to be off or everything breaks because I haven't been able to RE the protocol CRC / validation
      radio.setCRCLength(RF24_CRC_DISABLED); // disable CRC

      radio.setChannel(53); // remotes transmit at 2453

      radio.closeReadingPipe(0); // close pipes unless something left it listening
      radio.closeReadingPipe(1);
      radio.closeReadingPipe(2);
      radio.closeReadingPipe(3);
      radio.closeReadingPipe(4);
      radio.closeReadingPipe(5);
      Serial.println("Radio started");
    }
    else
    {
      Serial.println("Failure starting radio");
    }
  }
  return radioValid;
}

bool Directolor::enterRemoteSearchMode()
{
  if (radioStarted())
  {
    radio.stopListening();
    radio.setAddressWidth(5);
    radio.openReadingPipe(1, 0x5555555555); // always uses pipe 0
    Serial.println(F("searching for remote"));
    radio.startListening(); // put radio in RX mode
    learningRemote = true;
    radio.setPayloadSize(MAX_PAYLOAD_SIZE);
    // this->radio.printPrettyDetails(); // (larger) function that prints human readable data - only used for debugging
    memset(&remoteCode, 0, sizeof(remoteCode));
    return true;
  }
  return false;
}

void Directolor::enterRemoteCaptureMode()
{
  if (remoteCode.radioCode[0] || remoteCode.radioCode[1])
  {
    radio.stopListening();
    radio.setAddressWidth(3);

    union
    {
      uint32_t address;
      uint8_t bytes[4];
    } combined;

    combined.bytes[2] = remoteCode.radioCode[0];
    combined.bytes[1] = remoteCode.radioCode[1];
    combined.bytes[0] = 0xC0;

    radio.openReadingPipe(1, combined.address); // always uses pipe 0
    radio.openReadingPipe(0, 0xFFFFC0);

    radio.startListening(); // put radio in RX mode
    Serial.println(F("Capture Mode"));
  }
  else if (learningRemote)
  {
    enterRemoteSearchMode();
  }
  else
  {
    radio.powerDown();
  }
}

void Directolor::printData(char payload[], int start, int count, char *separator)
{
  for (int i = start; i < count + start; i++)
  {
    if (i != start && separator != " ")
      Serial.print(", ");
    Serial.print(separator);
    if (payload[i] < 16)
      Serial.print("0");
    Serial.print(payload[i], HEX);
  }
}

void Directolor::dumpCodes()
{
  Serial.println("/*");
  Serial.print("* Radio: ");
  printData((char *)remoteCode.radioCode, 0, sizeof(remoteCode.radioCode));
  Serial.println();
  Serial.println("*/");
  Serial.println(",{ ");
  Serial.print("  {");
  printData((char *)remoteCode.radioCode, 0, sizeof(remoteCode.radioCode), "0x");
  Serial.println("},");
  dumpCommand((char *)remoteCode.c_setFav, COMMAND_CODE_UNIQUE_BYTES, "Set Favorite", true, true);
  dumpCommand((char *)remoteCode.c_duplicate, DUPLICATE_CODE_UNIQUE_BYTES, "Duplicate", true, true);

  Serial.println("  { //Remote Codes");

  for (int i = 0; i < DIRECTOLOR_REMOTE_CHANNELS; i++)
  {
    bool AnyValues = false;
    for (int j = 0; j < COMMAND_CODE_UNIQUE_BYTES; j++)
      AnyValues |= (remoteCode.channelCodes[i].c_open[0][j] != 0);
    if (!AnyValues)
      continue;
    if (i != 0)
      Serial.print(",");
    Serial.print("   { //Remote Channel ");
    Serial.println(i + 1);
    dumpCommand((char *)remoteCode.channelCodes[i].c_open, COMMAND_CODE_UNIQUE_BYTES, "Open");
    dumpCommand((char *)remoteCode.channelCodes[i].c_close, COMMAND_CODE_UNIQUE_BYTES, "Close");
    dumpCommand((char *)remoteCode.channelCodes[i].c_tiltOpen, COMMAND_CODE_UNIQUE_BYTES, "Tilt Open");
    dumpCommand((char *)remoteCode.channelCodes[i].c_tiltClose, COMMAND_CODE_UNIQUE_BYTES, "Tilt Close");
    dumpCommand((char *)remoteCode.channelCodes[i].c_stop, COMMAND_CODE_UNIQUE_BYTES, "Stop");
    dumpCommand((char *)remoteCode.channelCodes[i].c_toFav, COMMAND_CODE_UNIQUE_BYTES, "Goto Favorite");
    dumpCommand((char *)remoteCode.channelCodes[i].c_join, GROUP_CODE_UNIQUE_BYTES, "Join Blind");
    dumpCommand((char *)remoteCode.channelCodes[i].c_remove, GROUP_CODE_UNIQUE_BYTES, "Remove Blind", false);
    Serial.print("    }");
    Serial.println();
  }
  Serial.println("   }");
  Serial.println("  }");
}

void Directolor::dumpCommand(char *codes, int bytes, char *command, bool includeComma, bool isFav)
{
  if (isFav)
    Serial.print("  {");
  else
    Serial.print("    {");
  for (int i = 0; i < UNIQUE_CODES_PER_ACTION; i++)
  {
    if (i != 0)
      Serial.print(",");
    Serial.print("{");
    printData(codes + (bytes * i), 0, bytes, "0x");
    Serial.print("}");
  }
  if (includeComma)
    Serial.print("}, // ");
  else
    Serial.print("}  // ");
  Serial.println(command);
}

void Directolor::foundCommandCode(uint8_t codes[], char payload[], int bytes, const uint8_t dynamicBytes[])
{
  int offset = -1;
  for (int i = 0; i < UNIQUE_CODES_PER_ACTION; i++)
  {
    bool alreadyPopulated = false;
    for (int j = 0; j < bytes; j++)
    {
      if (codes[(i * bytes) + j] != 0)
      {
        alreadyPopulated = true;
        break;
      }
    }
    if (alreadyPopulated)
      continue;
    offset = i;
    break;
  }
  if (offset == -1)
    offset = random(UNIQUE_CODES_PER_ACTION);

  for (int i = 0; i < bytes; i++)
  {
    codes[(offset * bytes) + i] = payload[dynamicBytes[i]];
  }
}

void Directolor::checkRadioPayload()
{
  if (radioStarted())
  {
    char payload[32];
    uint8_t pipe;
    if (radio.available(&pipe))
    {
      uint8_t bytes = radio.getPayloadSize(); // get the size of the payload
      radio.read(&payload, bytes);            // fetch payload from FIFO

      if (learningRemote)
      {
        Serial.print(".");
        uint8_t foundPattern = 0;

        for (int i = 0; i < bytes; i++)
        {
          if (payload[i] != matchPattern[foundPattern])
            foundPattern = 0;
          else
            foundPattern++;

          if (foundPattern > 3 && i > 4)
          {
            Serial.println();
            Serial.print(F("Found Remote with address: "));
            printData(payload, i - 5, 3);
            Serial.println();

            remoteCode.radioCode[0] = payload[i - 5];
            remoteCode.radioCode[1] = payload[i - 4];
            learningRemote = false;
            enterRemoteCaptureMode();
          }
        }
      }
      else
      {
        bytes = payload[0] + 4;
        if (bytes > 32)
          return;
#ifdef DIRECTOLOR_CAPTURE_FIRST
        bool skip = (millis() - lastMillis < 50);
        lastMillis = millis();
        if (skip)
          return;
#endif
        Serial.print(F("bytes: "));
        Serial.print(bytes - 1); // print the size of the payload
        Serial.print(F(" pipe: "));
        Serial.print(pipe); // print the pipe number
        Serial.print(F(": "));

        printData(payload, 0, bytes);

        Serial.print(" ");

        switch (payload[0])
        {
          uint8_t *commandGroup;
        case COMMAND_CODE_LENGTH:
          remoteCode.radioCode[2] = payload[6];
          remoteCode.radioCode[3] = payload[7];

          switch ((BlindAction)payload[16])
          {
          case directolor_open:
            Serial.print("Open");
            commandGroup = (uint8_t *)remoteCode.channelCodes[payload[11] - 1].c_open;
            break;
          case directolor_close:
            Serial.print("Close");
            commandGroup = (uint8_t *)remoteCode.channelCodes[payload[11] - 1].c_close;
            break;
          case directolor_tiltOpen:
            Serial.print("Tilt Open");
            commandGroup = (uint8_t *)remoteCode.channelCodes[payload[11] - 1].c_tiltOpen;
            break;
          case directolor_tiltClose:
            Serial.print("Tilt Close");
            commandGroup = (uint8_t *)remoteCode.channelCodes[payload[11] - 1].c_tiltClose;
            break;
          case directolor_stop:
            Serial.print("Stop");
            commandGroup = (uint8_t *)remoteCode.channelCodes[payload[11] - 1].c_stop;
            break;
          case directolor_toFav:
            Serial.print("to Fav");
            commandGroup = (uint8_t *)remoteCode.channelCodes[payload[11] - 1].c_toFav;
            break;
          };
          foundCommandCode(commandGroup, payload, COMMAND_CODE_UNIQUE_BYTES, commandDynamicBytesRead);
          break;
        case GROUP_CODE_LENGTH:
          switch ((BlindAction)payload[10])
          {
          case directolor_join:
            Serial.print("Join");
            commandGroup = (uint8_t *)remoteCode.channelCodes[payload[9] - 1].c_join;
            break;
          case directolor_remove:
            Serial.print("Remove");
            commandGroup = (uint8_t *)remoteCode.channelCodes[payload[9] - 1].c_remove;
            break;
          }
          foundCommandCode(commandGroup, payload, GROUP_CODE_UNIQUE_BYTES, groupDynamicBytesRead);
          break;
        case STORE_FAV_CODE_LENGTH:
          Serial.print("Store Favorite");
          foundCommandCode((uint8_t *)remoteCode.c_setFav, payload, STORE_FAV_UNIQUE_BYTES, storeFavDynamicBytesRead);
          break;
        case DUPLICATE_CODE_LENGTH:
          Serial.print("Duplicate");
          foundCommandCode((uint8_t *)remoteCode.c_duplicate, payload, DUPLICATE_CODE_UNIQUE_BYTES, duplicateDynamicBytesRead);
          break;
        };
        Serial.println(); // print the payload's value
      }
    }
  }
}

bool Directolor::sendCode(int remoteId, uint8_t channel, BlindAction blindAction)
{
  if (remoteId < 1 || remoteId > DIRECTOLOR_REMOTE_COUNT || channel < 1 || channel > DIRECTOLOR_REMOTE_CHANNELS)
    return false;

  if (!radioStarted())
    return false;

  uint8_t *uniqueBytes;

  Serial.print("Remote ");
  Serial.print(remoteId);
  Serial.print(", Channel ");
  Serial.print(channel);
  Serial.print("-");

  CommandItem commandItem;
  commandItem.payloadSize = sizeof(commandPrototype);

  switch (blindAction)
  {
  case directolor_open:
    Serial.println("Open");
    uniqueBytes = (uint8_t *)remoteCodes[remoteId - 1].channelCodes[channel - 1].c_open;
    setCommandCodes((uint8_t *)commandItem.payloads, (uint8_t *)remoteCodes[remoteId - 1].radioCode, uniqueBytes, blindAction, channel);
    break;
  case directolor_close:
    Serial.println("Close");
    uniqueBytes = (uint8_t *)remoteCodes[remoteId - 1].channelCodes[channel - 1].c_close;
    setCommandCodes((uint8_t *)commandItem.payloads, (uint8_t *)remoteCodes[remoteId - 1].radioCode, uniqueBytes, blindAction, channel);
    break;
  case directolor_stop:
    Serial.println("Stop");
    uniqueBytes = (uint8_t *)remoteCodes[remoteId - 1].channelCodes[channel - 1].c_stop;
    setCommandCodes((uint8_t *)commandItem.payloads, (uint8_t *)remoteCodes[remoteId - 1].radioCode, uniqueBytes, blindAction, channel);
    break;
  case directolor_tiltOpen:
    Serial.println("Tilt Open");
    uniqueBytes = (uint8_t *)remoteCodes[remoteId - 1].channelCodes[channel - 1].c_tiltOpen;
    setCommandCodes((uint8_t *)commandItem.payloads, (uint8_t *)remoteCodes[remoteId - 1].radioCode, uniqueBytes, blindAction, channel);
    break;
  case directolor_tiltClose:
    Serial.println("Tilt Close");
    uniqueBytes = (uint8_t *)remoteCodes[remoteId - 1].channelCodes[channel - 1].c_tiltClose;
    setCommandCodes((uint8_t *)commandItem.payloads, (uint8_t *)remoteCodes[remoteId - 1].radioCode, uniqueBytes, blindAction, channel);
    break;
  case directolor_toFav:
    Serial.println("to Fav");
    uniqueBytes = (uint8_t *)remoteCodes[remoteId - 1].channelCodes[channel - 1].c_toFav;
    setCommandCodes((uint8_t *)commandItem.payloads, (uint8_t *)remoteCodes[remoteId - 1].radioCode, uniqueBytes, blindAction, channel);
    break;
  case directolor_join:
    sendCode(remoteId, channel, directolor_duplicate);
    Serial.println("JOIN");
    commandItem.payloadSize = sizeof(groupPrototype);
    uniqueBytes = (uint8_t *)remoteCodes[remoteId - 1].channelCodes[channel - 1].c_join;
    setGroupCodes((uint8_t *)commandItem.payloads, (uint8_t *)remoteCodes[remoteId - 1].radioCode, uniqueBytes, blindAction, channel);
    break;
  case directolor_remove:
    sendCode(remoteId, channel, directolor_duplicate);
    Serial.println("REMOVE");
    commandItem.payloadSize = sizeof(groupPrototype);
    uniqueBytes = (uint8_t *)remoteCodes[remoteId - 1].channelCodes[channel - 1].c_remove;
    setGroupCodes((uint8_t *)commandItem.payloads, (uint8_t *)remoteCodes[remoteId - 1].radioCode, uniqueBytes, blindAction, channel);
    break;
  case directolor_duplicate:
    Serial.println("DUPLICATE");
    commandItem.payloadSize = sizeof(duplicatePrototype);
    uniqueBytes = (uint8_t *)remoteCodes[remoteId - 1].c_duplicate;
    setDuplicateCodes((uint8_t *)commandItem.payloads, (uint8_t *)remoteCodes[remoteId - 1].radioCode, uniqueBytes, blindAction, channel);
  }

  xQueueSend(queue, &commandItem, portMAX_DELAY);
}

bool Directolor::sendCode(byte *payload, uint8_t payload_size)
{
#ifdef DIRECTOLOR_DEBUG_SENT_CODES
  Serial.print(payload_size);
  for (int i = 0; i < payload_size; i++)
  {
    Serial.print(" ");
    if (payload[i] < 16)
      Serial.print("0");
    Serial.print(payload[i], HEX);
  }
  Serial.println();
#endif
  radio.setPayloadSize(payload_size);

  for (int i = 0; i < MESSAGE_SEND_RETRIES; i++) // setting this too low failed intermittently
  {
    radio.writeFast(payload, payload_size, true);
    delayMicroseconds(1); // removing this made it not work
  }
}

void Directolor::setDuplicateCodes(uint8_t *payload, uint8_t *radioCode, uint8_t *uniqueBytes, BlindAction blindAction, uint8_t channel)
{
  for (int i = 0; i < UNIQUE_CODES_PER_ACTION; i++)
  {
    const uint8_t offset = 6;
    int payloadOffset = i * MAX_PAYLOAD_SIZE;
    int uniqueBytesOffset = i * DUPLICATE_CODE_UNIQUE_BYTES;
    for (int j = 0; j < sizeof(duplicatePrototype); j++)
    {
      switch (j) // 6, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23
      {
      case 6 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 0];
        break;
      case 9 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 1];
        break;
      case 10 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 2];
        break;
      case 11 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 3];
        break;
      case 12 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 4];
        break;
      case 13 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 5];
        break;
      case 14 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 6];
        break;
      case 15 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 7];
        break;
      case 16 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 8];
        break;
      case 17 + offset:
        payload[payloadOffset + j] = radioCode[1];
        break;
      case 18 + offset:
        payload[payloadOffset + j] = radioCode[0];
        break;
      case 19 + offset:
        payload[payloadOffset + j] = radioCode[2];
        break;
      case 20 + offset:
        payload[payloadOffset + j] = radioCode[3];
        break;
      case 22 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 9];
        break;
      case 23 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 10];
        break;
      default:
        payload[payloadOffset + j] = duplicatePrototype[j];
        break;
      }
    }
  }
}

void Directolor::setGroupCodes(uint8_t *payload, uint8_t *radioCode, uint8_t *uniqueBytes, BlindAction blindAction, uint8_t channel)
{
  for (int i = 0; i < UNIQUE_CODES_PER_ACTION; i++)
  {
    const uint8_t offset = 6;
    int payloadOffset = i * MAX_PAYLOAD_SIZE;
    int uniqueBytesOffset = i * GROUP_CODE_UNIQUE_BYTES;
    for (int j = 0; j < sizeof(groupPrototype); j++)
    {
      switch (j) // 0, 1, 6, 9, 10, 12, 13, 14, 15
      {
      case 0 + offset:
        payload[payloadOffset + j] = radioCode[0];
        break;
      case 1 + offset:
        payload[payloadOffset + j] = radioCode[1];
        break;
      case 6 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 0];
        break;
      case 9 + offset:
        payload[payloadOffset + j] = radioCode[2];
        break;
      case 10 + offset:
        payload[payloadOffset + j] = radioCode[3];
        break;
      case 12 + offset:
        payload[payloadOffset + j] = channel;
        break;
      case 13 + offset:
        payload[payloadOffset + j] = blindAction;
        break;
      case 14 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 1];
        break;
      case 15 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 2];
        break;
      default:
        payload[payloadOffset + j] = groupPrototype[j];
        break;
      }
    }
  }
}

void Directolor::setCommandCodes(uint8_t *payload, uint8_t *radioCode, uint8_t *uniqueBytes, BlindAction blindAction, uint8_t channel)
{
  for (int i = 0; i < UNIQUE_CODES_PER_ACTION; i++)
  {
    const uint8_t offset = 6;
    int payloadOffset = i * MAX_PAYLOAD_SIZE;
    int uniqueBytesOffset = i * COMMAND_CODE_UNIQUE_BYTES;
    for (int j = 0; j < sizeof(commandPrototype); j++)
    {
      switch (j) // 3, 4, 9, 12, 13, 16, 19, 20, 22, 24, 25
      {
      case 0 + offset:
        payload[payloadOffset + j] = radioCode[0];
        break;
      case 1 + offset:
        payload[payloadOffset + j] = radioCode[1];
        break;
      case 6 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 0];
        break;
      case 9 + offset:
        payload[payloadOffset + j] = radioCode[2];
        break;
      case 10 + offset:
        payload[payloadOffset + j] = radioCode[3];
        break;
      case 13 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 1];
        break;
      case 14 + offset:
        payload[payloadOffset + j] = channel;
        break;
      case 16 + offset:
        payload[payloadOffset + j] = radioCode[2];
        break;
      case 17 + offset:
        payload[payloadOffset + j] = radioCode[3];
        break;
      case 19 + offset:
        payload[payloadOffset + j] = blindAction;
        break;
      case 21 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 2];
        break;
      case 22 + offset:
        payload[payloadOffset + j] = uniqueBytes[uniqueBytesOffset + 3];
        break;
      default:
        payload[payloadOffset + j] = commandPrototype[j];
        break;
      }
    }
  }
}

void Directolor::sendCodeTask(void *parameter)
{
  CommandItem commandItem;

  while (1)
  {
    if (xQueueReceive(queue, &commandItem, 1))
    {
      if (!messageIsSending)
      {
        messageIsSending = true;
        radio.powerUp();
        radio.stopListening(); // put radio in TX mode
        delay(20);             // the first command seems to be weak...
        radio.setPALevel(RF24_PA_MAX);
        radio.setAddressWidth(3);
        radio.enableDynamicAck();
        radio.openWritingPipe(0x060406);
      }

      unsigned long start_timer = millis();

      int lastCode = -1;
      int code = random(UNIQUE_CODES_PER_ACTION);

      for (int i = 0; i < MESSAGE_SEND_ATTEMPTS; i++)
      {
        while (code == lastCode)
          code = random(UNIQUE_CODES_PER_ACTION);

        sendCode(commandItem.payloads[code], commandItem.payloadSize);
        delay(10); // feed the watchdog timer in the idle thread....
      }
      unsigned long end_timer = millis();
      Serial.println(end_timer - start_timer);
      delayMicroseconds(500);
    }
      else
      {
        if (messageIsSending)
        {
          messageIsSending = false;
          enterRemoteCaptureMode();
        }
        checkRadioPayload();
      }
    }
  }
