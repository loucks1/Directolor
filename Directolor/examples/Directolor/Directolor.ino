#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "Directolor.h"

const char *ssid = "YourSSIDHere";
const char *password = "YourPasswordHere";

WebServer server(80);
Directolor directolor(22, 21);

const int led = 13;

void handleRoot() {
  digitalWrite(led, 1);
  char temp[2000];

  char remoteOptions[60 * DIRECTOLOR_REMOTE_COUNT];
  char channelOptions[65 * DIRECTOLOR_REMOTE_CHANNELS];

  int offset = 0;

  char tempValue[15];

  short int remote = 1;
  short int channel = 0;
  bool sendMultiCode = false;
  String action;
  String type;

  if (server.hasArg("remote"))
    remote = server.arg("remote").toInt();
  if (server.hasArg("action"))
    action = server.arg("action");
  if (server.hasArg("type"))
    type = server.arg("type");
  if (server.hasArg("channel")) {
    channel = server.arg("channel").toInt();
  }
  else if (server.hasArg("channels")) {
    channel = server.arg("channels").toInt();
    sendMultiCode = true;
  }
  else  {
    for (int i = 0; i < DIRECTOLOR_REMOTE_CHANNELS; i++)
    {
      snprintf(&tempValue[0], 15, "c%d", i + 1);
      if (server.hasArg(tempValue))
        channel += pow(2, i);
    }
    if (channel == 0) {
      channel = 1;
    }
    else {
      sendMultiCode = true;
    }
  }
  //handle params
  if (sendMultiCode) {
    if (action.equalsIgnoreCase("open"))
      directolor.sendMultiChannelCode(remote, channel, directolor_open);
    else if (action.equalsIgnoreCase("close"))
      directolor.sendMultiChannelCode(remote, channel, directolor_close);
    else if (action.equalsIgnoreCase("tiltOpen"))
      directolor.sendMultiChannelCode(remote, channel, directolor_tiltOpen);
    else if (action.equalsIgnoreCase("tiltClose"))
      directolor.sendMultiChannelCode(remote, channel, directolor_tiltClose);
    else if (action.equalsIgnoreCase("stop"))
      directolor.sendMultiChannelCode(remote, channel, directolor_stop);
    else if (action.equalsIgnoreCase("toFav"))
      directolor.sendMultiChannelCode(remote, channel, directolor_toFav);
    else if (action.equalsIgnoreCase("join"))
      directolor.sendMultiChannelCode(remote, channel, directolor_join);
    else if (action.equalsIgnoreCase("remove"))
      directolor.sendMultiChannelCode(remote, channel, directolor_remove);
    else if (action.equalsIgnoreCase("setFav"))
      directolor.sendMultiChannelCode(remote, channel, directolor_setFav);
    else if (action.equalsIgnoreCase("duplicate"))
      directolor.sendMultiChannelCode(remote, channel, directolor_duplicate);
  }
  else  {
    if (action.equalsIgnoreCase("open"))
      directolor.sendCode(remote, channel, directolor_open);
    else if (action.equalsIgnoreCase("close"))
      directolor.sendCode(remote, channel, directolor_close);
    else if (action.equalsIgnoreCase("tiltOpen"))
      directolor.sendCode(remote, channel, directolor_tiltOpen);
    else if (action.equalsIgnoreCase("tiltClose"))
      directolor.sendCode(remote, channel, directolor_tiltClose);
    else if (action.equalsIgnoreCase("stop"))
      directolor.sendCode(remote, channel, directolor_stop);
    else if (action.equalsIgnoreCase("toFav"))
      directolor.sendCode(remote, channel, directolor_toFav);
    else if (action.equalsIgnoreCase("join"))
      directolor.sendCode(remote, channel, directolor_join);
    else if (action.equalsIgnoreCase("remove"))
      directolor.sendCode(remote, channel, directolor_remove);
    else if (action.equalsIgnoreCase("setFav"))
      directolor.sendCode(remote, channel, directolor_setFav);
    else if (action.equalsIgnoreCase("duplicate"))
      directolor.sendCode(remote, channel, directolor_duplicate);
  }

  //create webpage
  for (int i = 1; i < DIRECTOLOR_REMOTE_COUNT + 1; i++)
  {
    if (i == remote)
      strcpy (tempValue, " selected");
    else
      tempValue[0] = 0;
    offset += snprintf(&remoteOptions[offset], 3000, "<option value=\"%d\"%s>Remote %d</option>", i, tempValue, i);
  }
  offset = 0;
  for (int i = 1; i < DIRECTOLOR_REMOTE_CHANNELS + 1; i++)
  {
    if ((short int)pow(2, (i - 1)) & channel)
      strcpy (tempValue, " checked");
    else
      tempValue[0] = 0;
    offset += snprintf(&channelOptions[offset], 3000, "<label>   %d:</label><input type=\"checkbox\" name=\"c%d\" %s>", i, i, tempValue);
  }

  snprintf(temp, 2000,
           "<html>\
  <head>\
    <title>directolor</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />\
  </head>\
  <body>\
    <h1>Welcome to directolor!</h1>\
<form action=\"/\">\
  <label>channel%s=%d</label>\
  <br><br>\
  <label for=\"Remotes\">Choose remote:</label>\
  <select name=\"remote\" id=\"remote\">\
    %s\
  </select><br><br>\
    <label for=\"Channel\">Choose channels:</label>\
           %s\
           <br><br>\
  <button type=\"submit\" name=\"action\" value=\"open\">Open</button>\
  <button type=\"submit\" name=\"action\" value=\"close\">Close</button>\
  <button type=\"submit\" name=\"action\" value=\"tiltOpen\">Tilt Open</button>\
  <button type=\"submit\" name=\"action\" value=\"tiltClose\">Tilt Close</button>\
  <br><br>\
  <button type=\"submit\" name=\"action\" value=\"stop\">Stop</button>\
  <button type=\"submit\" name=\"action\" value=\"toFav\">to Favorite</button>\
  <br><br>\
  <button type=\"submit\" name=\"action\" value=\"join\">Join</button>\
  <button type=\"submit\" name=\"action\" value=\"remove\">Remove</button>\
  <button type=\"submit\" name=\"action\" value=\"setFav\">Set Favorite</button>\
  <button type=\"submit\" name=\"action\" value=\"duplicate\">Duplicate</button>\
</form>\
  </body>\
</html>", sendMultiCode ? "s" : "", channel, remoteOptions, channelOptions );
  server.send(200, "text/html", temp);
  digitalWrite(led, 0);
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void setup(void) {
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println();

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);


  Serial.println();
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("directolor")) {     //should be able to access this from the local network at http://directolor
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
  directolor.processLoop();
}
