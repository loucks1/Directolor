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
#include "CRC.h"

RF24 Directolor::radio;
QueueHandle_t Directolor::queue;
bool Directolor::messageIsSending = false;
bool Directolor::learningRemote = false;
bool Directolor::radioValid = false;
Directolor::RemoteCode Directolor::remoteCode;
long Directolor::lastMillis = 0;

constexpr uint8_t Directolor::matchPattern[4] = {0xC0, 0X11, 0X00, 0X05}; // this is what we use to find out what the codes for the remote are

Directolor::Directolor(uint16_t _cepin, uint16_t _cspin, uint32_t _spi_speed)
{
  radio = RF24(_cepin, _cspin, _spi_speed);
  queue = xQueueCreate(DIRECTOLOR_REMOTE_COUNT, sizeof(CommandItem));
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
  Serial.println("}}");
}

void Directolor::checkRadioPayload()  //could modify this to allow capture if commands were sent using multiple channels
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
        bool skip = (millis() - lastMillis < 25);
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
            break;
          case directolor_close:
            Serial.print("Close");
            break;
          case directolor_tiltOpen:
            Serial.print("Tilt Open");
            break;
          case directolor_tiltClose:
            Serial.print("Tilt Close");
            break;
          case directolor_stop:
            Serial.print("Stop");
            break;
          case directolor_toFav:
            Serial.print("to Fav");
            break;
          };
          break;
        case GROUP_CODE_LENGTH:
          switch ((BlindAction)payload[10])
          {
          case directolor_join:
            Serial.print("Join");
            break;
          case directolor_remove:
            Serial.print("Remove");
            break;
          }
          break;
        case STORE_FAV_CODE_LENGTH:
          Serial.print("Store Favorite");
          break;
        case DUPLICATE_CODE_LENGTH:
          Serial.print("Duplicate");
          break;
        };
        Serial.println(); // print the payload's value
      }
    }
  }
}

bool Directolor::sendCode(int remoteId, uint8_t channel, BlindAction blindAction)
{
  return sendMultiChannelCode(remoteId, pow(2, channel - 1), blindAction);
}

bool Directolor::sendMultiChannelCode(int remoteId, uint8_t channels, BlindAction blindAction)
{
  if (channels & 0xC0 || remoteId < 1 || remoteId > DIRECTOLOR_REMOTE_COUNT)
    return false;

  if (!radioStarted())
    return false;

  CommandItem commandItems[DIRECTOLOR_REMOTE_COUNT];
  int queuedCommands = 0;

  while (xQueueReceive(queue, &commandItems[queuedCommands], 0))
    queuedCommands++;

  uint8_t *uniqueBytes;

  Serial.print("Remote ");
  Serial.print(remoteId);
  Serial.print(", Channels:");
  for (int i = 0; i < 6; i++)
    if (bitRead(channels, i))
    {
      Serial.print(" ");
      Serial.print(i + 1);
    }
  Serial.print("-");

  CommandItem commandItem;
  commandItem.radioCodes = (uint8_t *)remoteCodes[remoteId - 1].radioCode;

  switch (blindAction)
  {
  case directolor_open:
    Serial.println("Open");
    break;
  case directolor_close:
    Serial.println("Close");
    break;
  case directolor_stop:
    Serial.println("Stop");
    break;
  case directolor_tiltOpen:
    Serial.println("Tilt Open");
    break;
  case directolor_tiltClose:
    Serial.println("Tilt Close");
    break;
  case directolor_toFav:
    Serial.println("to Fav");
    break;
  case directolor_join:
    sendCode(remoteId, channels, directolor_duplicate);
    Serial.println("JOIN");
    break;
  case directolor_remove:
    sendCode(remoteId, channels, directolor_duplicate);
    Serial.println("REMOVE");
    break;
  case directolor_duplicate:
    Serial.println("DUPLICATE");
  }

  bool commandQueued = false;
  for (int i = 0; i < queuedCommands; i++)
  {
    if (commandItems[i].radioCodes == commandItem.radioCodes && commandItems[i].blindAction == blindAction)
    {
      commandItems[i].channels |= channels;
      commandItems[i].resendRemainingCount = MESSAGE_SEND_ATTEMPTS;
      commandQueued = true;
    }
    xQueueSend(queue, &commandItems[i], portMAX_DELAY);
  }

  if (!commandQueued)
  {
    commandItem.channels = channels;
    commandItem.blindAction = blindAction;
    commandItem.resendRemainingCount = MESSAGE_SEND_ATTEMPTS;
    xQueueSend(queue, &commandItem, portMAX_DELAY);
  }
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
    radio.writeFast(payload, payload_size, true);  //we aren't waiting for an ACK, so we need to writeFast with multiCast set to true 
    delayMicroseconds(1); // removing this made it not work
  }
}

// There are a lot of hardcoded values here.  I'm unsure why these ever might need to be different.
// Here are codes I gathered from my remotes
// remote 1
// 12 80 0D 55 FF FF C4 05 B1 EC 1D E3 98 8B C7 52 75 A9 C8 B1 DA
// 12 80 0D 06 FF FF C4 05 B1 EC 1D E3 98 8B C7 52 75 A9 C8 A4 B1
// 12 80 0D 02 FF FF C4 05 B1 EC 1D E3 98 8B C7 52 75 A9 C8 68 B3

// remote 2
// 12 80 0D 85 FF FF BD 55 08 4F 65 60 B0 B0 77 FA 2D FD C8 E2 2E
// 12 80 0D FF FF FF BD 55 08 4F 65 60 B0 B0 77 FA 2D FD C8 B9 26

// They always send the same value, per remote, and I think it might be some sort of MAC address or something.  Anyway, doesn't appear to need to be different and the generated duplicate codes from these random values seem to work just fine....

static constexpr uint8_t duplicatePrototype[] = {0XFF, 0XFF, 0xC0, 0X12, 0X80, 0X0D, 0x67, 0XFF, 0XFF, 0XC4, 0X05, 0XB1, 0XEC, 0X1D, 0XE3, 0X98, 0x8B, 0X2D, 0XDE, 0X00, 0XEF, 0XC8}; // 6, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23

int Directolor::getDuplicateRadioCommand(byte *payload, CommandItem commandItem)
{
  const uint8_t offset = 0;
  int payloadOffset = 0;
  // int uniqueBytesOffset = i * DUPLICATE_CODE_UNIQUE_BYTES;
  for (int j = 0; j < sizeof(duplicatePrototype); j++)
  {
    switch (j) // 6, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23
    {
    case 6 + offset:
      payload[payloadOffset + j] = random(256);
      break;
    case 9 + offset:
      payload[payloadOffset + j] = 0x06;
      break;
    case 10 + offset:
      payload[payloadOffset + j] = 0x03;
      break;
    case 11 + offset:
      payload[payloadOffset + j] = 0x20;
      break;
    case 12 + offset:
      payload[payloadOffset + j] = 0x05;
      break;
    case 13 + offset:
      payload[payloadOffset + j] = 0x12;
      break;
    case 14 + offset:
      payload[payloadOffset + j] = 0x03;
      break;
    case 15 + offset:
      payload[payloadOffset + j] = 0xAC;
      break;
    case 16 + offset:
      payload[payloadOffset + j] = 0x56;
      break;
    case 17 + offset:
      payload[payloadOffset + j] = commandItem.radioCodes[1];
      break;
    case 18 + offset:
      payload[payloadOffset + j] = commandItem.radioCodes[0];
      break;
    case 19 + offset:
      payload[payloadOffset + j] = commandItem.radioCodes[2];
      break;
    case 20 + offset:
      payload[payloadOffset + j] = commandItem.radioCodes[3];
      break;
    default:
      payload[payloadOffset + j] = duplicatePrototype[j];
      break;
    }
  }
  return sizeof(duplicatePrototype);
}

static constexpr uint8_t groupPrototype[] = {0X11, 0X11, 0xC0, 0X0A, 0X40, 0X05, 0X18, 0XFF, 0XFF, 0X8A, 0X91, 0X08, 0X03, 0X01}; // 0, 1, 6, 9, 10, 12, 13, 14, 15

int Directolor::getGroupRadioCommand(byte *payload, CommandItem commandItem)
{
  const uint8_t offset = 0;
  int payloadOffset = 0;
  for (int j = 0; j < sizeof(groupPrototype); j++)
  {
    switch (j) // 0, 1, 6, 9, 10, 12, 13, 14, 15
    {
    case 0 + offset:
      payload[payloadOffset + j] = commandItem.radioCodes[0];
      break;
    case 1 + offset:
      payload[payloadOffset + j] = commandItem.radioCodes[1];
      break;
    case 6 + offset:
      payload[payloadOffset + j] = random(256);
      break;
    case 9 + offset:
      payload[payloadOffset + j] = commandItem.radioCodes[2];
      break;
    case 10 + offset:
      payload[payloadOffset + j] = commandItem.radioCodes[3];
      break;
    case 12 + offset:
      for (int i = 5; i >= 0; i--)
      {
        if (bitRead(commandItem.channels, i))
        {
          payload[payloadOffset + j] = i + 1;
        }
      }
      break;
    case 13 + offset:
      payload[payloadOffset + j] = commandItem.blindAction;
      break;
    default:
      payload[payloadOffset + j] = groupPrototype[j];
      break;
    }
  }
  return sizeof(groupPrototype);
}

static constexpr uint8_t commandPrototype[] = {0X11, 0X11, 0xC0, 0X10, 0X00, 0X05, 0XBC, 0XFF, 0XFF, 0X8A, 0X91, 0X86, 0X06, 0X99, 0X01, 0X00, 0X8A, 0X91, 0X52, 0X53, 0X00};

int Directolor::getRadioCommand(byte *payload, CommandItem commandItem)
{
  switch (commandItem.blindAction)
  {
  case directolor_join:
  case directolor_remove:
    return getGroupRadioCommand(payload, commandItem);
  case directolor_duplicate:
    return getDuplicateRadioCommand(payload, commandItem);
  }
  const uint8_t offset = 0;
  int payloadOffset = 0;
  for (int j = 0; j < MAX_PAYLOAD_SIZE; j++)
  {
    switch (j) // 0, 1, 6, 9, 10, 12, 13, 14, 15
    {
    case 0 + offset:
      payload[payloadOffset + j] = commandItem.radioCodes[0];
      break;
    case 1 + offset:
      payload[payloadOffset + j] = commandItem.radioCodes[1];
      break;
    case 6 + offset:
      payload[payloadOffset + j] = random(256);
      break;
    case 9 + offset:
      payload[payloadOffset + j] = commandItem.radioCodes[2];
      break;
    case 10 + offset:
      payload[payloadOffset + j] = commandItem.radioCodes[3];
      break;
    case 13 + offset:
      payload[payloadOffset + j] = random(256);
      break;
    case 14 + offset:
      for (int i = 0; i < 6; i++)
      {
        if (bitRead(commandItem.channels, i))
        {
          payload[payloadOffset + j] = i + 1;
          payload[3] = payload[3] + 1;
          payloadOffset += 1;
        }
      }
      payloadOffset -= 1;
      break;
    case 16 + offset:
      payload[payloadOffset + j] = commandItem.radioCodes[2];
      break;
    case 17 + offset:
      payload[payloadOffset + j] = commandItem.radioCodes[3];
      break;
    case 19 + offset:
      payload[payloadOffset + j] = commandItem.blindAction;
      break;
    default:
      payload[payloadOffset + j] = commandPrototype[j];
      break;
    }
  }

  return sizeof(commandPrototype) + payloadOffset;
}

long lastMessageSend = 0;
long lastInhibit = 0;
int lastInhibitDuration = 0;

void Directolor::sendCodeTask(void *parameter)
{
  Directolor::CommandItem commandItem;

  while (1)
  {
    if ((abs(millis() - lastMessageSend) > INTERMESSAGE_SEND_DELAY) && (abs(millis() - lastInhibit) > lastInhibitDuration) && xQueueReceive(queue, &commandItem, 1))
    {
      messageIsSending = true;
      radio.powerUp();
      radio.stopListening(); // put radio in TX mode
      delay(20);             // the first command seems to be weak...
      radio.setPALevel(RF24_PA_MAX);
      radio.setAddressWidth(3);
      radio.enableDynamicAck();
      radio.openWritingPipe(0x060406);

      unsigned long start_timer = millis();

      byte payload[MAX_PAYLOAD_SIZE];

      int length = getRadioCommand(payload, commandItem);

      uint16_t crc = crc16((uint8_t *)payload, length, 0x755b, 0xFFFF, 0, false, false); // took some time to figure this out.  big thanks to CRC RevEng by Gregory Cook!!!!  CRC is calculated over the whole payload, including radio id at start.
      payload[length++] = crc >> 8;
      payload[length] = crc & 0xFF;

      for (int i = MAX_PAYLOAD_SIZE; i > 0; i--)  //pad with leading 0x55 to train the shade receivers
      {
        if (i - (MAX_PAYLOAD_SIZE - length) >= 0)
          payload[i - 1] = payload[i - (MAX_PAYLOAD_SIZE - length)];
        else
          payload[i - 1] = 0x55;
      }

      sendCode(payload, MAX_PAYLOAD_SIZE);

      unsigned long end_timer = millis();
      Serial.println(end_timer - start_timer);
      lastMessageSend = millis();
      if (commandItem.blindAction == directolor_duplicate) // join / remove require duplicate to immediately preceed.
        lastMessageSend = 0;
      if (--commandItem.resendRemainingCount > 0)
        xQueueSend(queue, &commandItem, portMAX_DELAY);
      delay(10); // feed the watchdog timer in the idle thread....
    }

    if (messageIsSending)
    {
      messageIsSending = false;
      enterRemoteCaptureMode();
    }
    checkRadioPayload();
    yield();
  }
}

void Directolor::inhibitSend(int durationMS)
{
  if (durationMS > INTERMESSAGE_SEND_DELAY * 4)
    durationMS = INTERMESSAGE_SEND_DELAY * 4;
  if (durationMS > 0)
  {
    while (messageIsSending)
      delay(1);
    lastInhibitDuration = durationMS;
    lastInhibit = millis();
  }
  else
  {
    enableSend();
  }
}

void Directolor::enableSend()
{
  lastInhibitDuration = 0;
  lastInhibit = 0;
}