/**
   Run this and monitor the serial output.
   You will need to connect a NRF24L01+ via the SPI on pins 22 & 21 - you'll need more connected.
   Follow the NRF24L01+ instructions for connecting.
   When it runs it will enter search mode.  Choose a single channel on your remote - press the open button
   You may need to press it a few times to get it found
   Once you have found it, then just press keys until you've pressed the keys 3 times each.  Do this for each channel as well as join, remove and set favorite.

   Then press 'd' to "dump" the codes - this will be in the correct format to add to the Directolor library - don't forget to change the Maximum remotes, if needed.

   Works well on an ESP32 WROOM
*/

#include "Directolor.h"
#include <SerialCommands.h>

int remote = 1;
int channel = 1;

char serial_command_buffer_[32];
SerialCommands serial_commands_(&Serial, serial_command_buffer_, sizeof(serial_command_buffer_), "\n", " ");

Directolor directolor(22, 21);

void cmd_dump(SerialCommands* sender)
{
  directolor.dumpCodes();
}

void cmd_close(SerialCommands* sender)
{
  directolor.sendCode(remote, channel, directolor_close);
}

void cmd_open(SerialCommands* sender)
{
  directolor.sendCode(remote, channel, directolor_open);
}

void cmd_stop(SerialCommands* sender)
{
  directolor.sendCode(remote, channel, directolor_stop);
}

void cmd_join(SerialCommands* sender)
{
  directolor.sendCode(remote, channel, directolor_join);
}

void cmd_remove(SerialCommands* sender)
{
  directolor.sendCode(remote, channel, directolor_remove);
}

void cmd_remoteSearchMode(SerialCommands* sender)
{
  if (! directolor.enterRemoteSearchMode())
    Serial.println("Unable to enter remote capture mode...");
}

void cmd_updateRemote(SerialCommands* sender)
{
  char* value = sender->Next();
  if (value == NULL)
  {
    sender->GetSerial()->println("ERROR No Remote");
    return;
  }
  int rem = atoi(value);
  if (rem < 1 || rem > DIRECTOLOR_REMOTE_COUNT)
  {
    sender->GetSerial()->print("ERROR Remote must be between 1 and ");
    sender->GetSerial()->print(DIRECTOLOR_REMOTE_COUNT);
    sender->GetSerial()->println(" (inclusive)");
    return; 
  }
  remote = rem;
  cmd_help(sender);
}

void cmd_updateChannel(SerialCommands* sender)
{
  char* value = sender->Next();
  if (value == NULL)
  {
    sender->GetSerial()->println("ERROR No Remote");
    return;
  }
  int chan = atoi(value);
  if (chan < 1 || chan > DIRECTOLOR_REMOTE_CHANNELS)
  {
    sender->GetSerial()->print("ERROR Channel must be between 1 and ");
    sender->GetSerial()->print(DIRECTOLOR_REMOTE_CHANNELS);
    sender->GetSerial()->println(" (inclusive)");
    return; 
  }
  channel = chan;
  cmd_help(sender);
}


void cmd_help(SerialCommands* sender)
{
  Serial.println("Commands available:\r\n"\
                 "(search) remote - enter Remote Search Mode\r\n"\
                 "(dump) codes    - dump the remote code so you can put into directolor.h (only valid if you've captured a remote code using search)\r\n"\
                 "(o)pen blind    - send open code for current channel(s)\r\n"\
                 "(c)lose blind   - send close code for current channel(s)\r\n"\
                 "(s)top blind    - send stop code for current channel(s)\r\n"\
                 "(j)oin blind    - send join code for current channel\r\n"\
                 "(r)emove blind  - send remove code for current channel\r\n"\
                 "(remote #)      - set remote number\r\n"\
                 "(channel #)     - set channel number\r\n"\
                 "(help)          - show this screen\r\n"\
                );
  Serial.println();
  Serial.print("Remote = ");
  Serial.print(remote);
  Serial.print(", Channel = ");
  Serial.println(channel);
}

void cmd_unrecognized(SerialCommands* sender, const char* cmd)
{
  sender->GetSerial()->print("ERROR: Unrecognized command [");
  sender->GetSerial()->print(cmd);
  sender->GetSerial()->println("]");
}

SerialCommand cmd_dump_("dump", cmd_dump);
SerialCommand cmd_close_("c", cmd_close);
SerialCommand cmd_open_("o", cmd_open);
SerialCommand cmd_stop_("s", cmd_stop);
SerialCommand cmd_join_("j", cmd_join);
SerialCommand cmd_remove_("r", cmd_remove);
SerialCommand cmd_remoteSearchMode_("search", cmd_remoteSearchMode);
SerialCommand cmd_updateRemote_("remote", cmd_updateRemote);
SerialCommand cmd_updateChannel_("channel", cmd_updateChannel);
SerialCommand cmd_help_("help", cmd_help);

void setup() {
  Serial.begin(115200);

  serial_commands_.SetDefaultHandler(&cmd_unrecognized);
  serial_commands_.AddCommand(&cmd_dump_);
  serial_commands_.AddCommand(&cmd_close_);
  serial_commands_.AddCommand(&cmd_open_);
  serial_commands_.AddCommand(&cmd_stop_);
  serial_commands_.AddCommand(&cmd_join_);
  serial_commands_.AddCommand(&cmd_remove_);
  serial_commands_.AddCommand(&cmd_remoteSearchMode_);
  serial_commands_.AddCommand(&cmd_updateRemote_);
  serial_commands_.AddCommand(&cmd_updateChannel_);
  serial_commands_.AddCommand(&cmd_help_);

  cmd_help(0);
}

void loop() {
  serial_commands_.ReadSerial();
} // loop
