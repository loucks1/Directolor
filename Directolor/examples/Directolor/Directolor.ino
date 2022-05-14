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

  char remoteOptions[400];
  char channelOptions[400];

  int offset = 0;

  char tempValue[15];

  short int remote = 1;
  short int channel = 1;

  if (server.hasArg("remote"))
    remote = server.arg("remote").toInt();
  if (server.hasArg("channel"))
    channel = server.arg("channel").toInt();

  if (server.hasArg("action"))
  {
    if (server.arg("action").equalsIgnoreCase("open"))
      directolor.sendCode(remote, channel, directolor_open);
    if (server.arg("action").equalsIgnoreCase("close"))
      directolor.sendCode(remote, channel, directolor_close);
    if (server.arg("action").equalsIgnoreCase("tiltOpen"))
      directolor.sendCode(remote, channel, directolor_tiltOpen);
    if (server.arg("action").equalsIgnoreCase("tiltClose"))
      directolor.sendCode(remote, channel, directolor_tiltClose);
    if (server.arg("action").equalsIgnoreCase("stop"))
      directolor.sendCode(remote, channel, directolor_stop);
    if (server.arg("action").equalsIgnoreCase("toFav"))
      directolor.sendCode(remote, channel, directolor_toFav);
    if (server.arg("action").equalsIgnoreCase("join"))
      directolor.sendCode(remote, channel, directolor_join);
    if (server.arg("action").equalsIgnoreCase("remove"))
      directolor.sendCode(remote, channel, directolor_remove);
    if (server.arg("action").equalsIgnoreCase("setFav"))
      directolor.sendCode(remote, channel, directolor_setFav);
    if (server.arg("action").equalsIgnoreCase("duplicate"))
      directolor.sendCode(remote, channel, directolor_duplicate);
  }

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
    if (i == channel)
      strcpy (tempValue, " selected");
    else
      tempValue[0] = 0;
    offset += snprintf(&channelOptions[offset], 3000, "<option value=\"%d\"%s>Channel %d</option>", i, tempValue, i);
  }

  snprintf(temp, 2000,

           "<html>\
  <head>\
    <title>directolor</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Welcome to directolor!</h1>\
<form action=\"/\">\
  <label for=\"Remotes\">Choose a remote:</label>\
  <select name=\"remote\" id=\"remote\">\
    %s\
  </select><br><br>\
    <label for=\"Channel\">Choose a channel:</label>\
  <select name=\"channel\" id=\"channel\">\
    %s\
  </select>\
  <br><br>\
  <button type=\"submit\" name=\"action\" value=\"open\">Open</button>\
  <button type=\"submit\" name=\"action\" value=\"close\">Close</button>\
  <button type=\"submit\" name=\"action\" value=\"tiltOpen\">Tilt Open</button>\
  <button type=\"submit\" name=\"action\" value=\"tiltClose\">Tilt Close</button>\
  <button type=\"submit\" name=\"action\" value=\"stop\">Stop</button>\
  <button type=\"submit\" name=\"action\" value=\"toFav\">to Favorite</button>\
  <br><br>\
  <button type=\"submit\" name=\"action\" value=\"join\">Join</button>\
  <button type=\"submit\" name=\"action\" value=\"remove\">Remove</button>\
  <button type=\"submit\" name=\"action\" value=\"setFav\">Set Favorite</button>\
  <button type=\"submit\" name=\"action\" value=\"duplicate\">Duplicate</button>\
</form>\
  </body>\
</html>", remoteOptions, channelOptions );
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
}
