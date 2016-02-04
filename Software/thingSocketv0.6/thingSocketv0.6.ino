/*
 *  This sketch is the source code for thngSocket.
 *  The sketch will search for SSID and Password in EEPROM and
 *  tries to connect to the AP using the SSID and Password.
 *  If it fails then it boots into AP mode and asks for SSID and Password from the user
 *  API for AP (SSID = thingSocket)
 *    http://192.168.4.1/a?ssid="yourSSID"&pass="yourPSKkey"
 *  A webpage is also provided for entering SSID and Password if you are using the browser method.
 *  The server will set a GPIO pin depending on the request
 *    http://server_ip/plug/read will read all the plug status,
 *    http://server_ip/plug/1/0 will set the GPIO14 low,
 *    http://server_ip/plug/1/1 will set the GPIO14 high
 *    http://server_ip/plug/2/0 will set the GPIO12 low,
 *    http://server_ip/plug/2/1 will set the GPIO12 high
 *    http://server_ip/plug/3/0 will set the GPIO13 low,
 *    http://server_ip/plug/3/1 will set the GPIO13 high
 *    http://server_ip/plug/4/0 will set the GPIO15 low,
 *    http://server_ip/plug/4/1 will set the GPIO15 high
 *    http://server_ip/factoryreset will clear the EEPROM contents. Its serves the purpose of factory resetting the device.
 *    http://server_ip/reboot will reboot the device after 10 seconds
 *    http://server_ip/setappliance will set the appliance location, type and name
 *  server_ip is the IP address of the ESP8266 module, will be
 *  printed to Serial when the module is connected.
 *  The complete project can be cloned @ https://github.com/automote/thingSocket.git
 *
 *  Inspired by:
 *  https://github.com/chriscook8/esp-arduino-apboot
 *  This example code is under GPL v3.
 *  modified 14 Jan 2016
 *  by Lovelesh Patel
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <EEPROM.h>

// GPIO 12,13,14,15 are smart plug GPIOs
#define PLUG_1 14
#define PLUG_2 12
#define PLUG_3 13
#define PLUG_4 15

#define BROADCAST_PORT 8888   // Port for sending general broadcast messages
#define NOTIFICATION_PORT 8000     // Port for notification broadcasts
// For debugging interface
#define DEBUG 1
#define MAX_RETRIES 20  // Max retries for checking wifi connection

MDNSResponder mdns;
// Create an instance of the Web server
// specify the port to listen on as an argument
WiFiServer server(80);

// Global Constant
const char* hardware_version = "v0.5";
const char* software_version = "v0.6";

// Global Variable
const char* APssid = "thingSocket";
String st;
String zone, appl_type, appl_name;
uint8_t MAC_array[6];
char MAC_char[18];
static unsigned char bcast[4] = { 255, 255, 255, 255 } ;   // broadcast IP address
unsigned int count = 0;

// Reboot flag to reboot the device when necessary
bool reboot_flag = false;

// Create an instance of the UDP server
WiFiUDP Udp;

// Function Declaration
void InitHardware(void);
void BroadcastSetup(void);
bool SSIDSearch(void);
void ZoneSearch(void);
bool TestWifi(void);
void WebServiceInit(void);
void MDNSService(void);
void WebServiceDaemon(bool webtype);
void WebService(bool webtype);
void SetupAP(void);
void Broadcast(void);
void NotificationBroadcast(int which_plug, int state);
void urlDecode(char *src);

void setup() {
  bool AP_required = false;
  // Initialise the hardware
  InitHardware();

  // Setting up the broadcast service
  BroadcastSetup();

  // Search for SSID and password from the EEPROM first and try to connect to AP
  AP_required = !SSIDSearch();
  ZoneSearch();

  // If it fails then make yourself AP and ask for SSID and password from user
  if (AP_required) {
    SetupAP();
  }
  // Initialise the webserver for direct initialization
  WebServiceInit();
}

void loop() {
  if (count % 1200 == 0) {
    // Test WiFi connection every minute and set the reboot flag if necessary
    // count is incremented roughly every 50ms
    Serial.println("checking wifi connection");
    reboot_flag = !TestWifi();
  }
  // Serving the requests from the client
  WebService(0);
  if (reboot_flag) {
    Serial.println("Rebooting device");
    delay(10000);
    ESP.restart();
  }
}

void InitHardware(void) {
  Serial.begin(115200);
  // Specify EEPROM block
  EEPROM.begin(512);
  delay(10);
  Serial.println();
  Serial.println("Setting up the Hardware");

  // prepare GPIO14, GPIO12, GPIO13, GPIO15
  pinMode(PLUG_1, OUTPUT);
  digitalWrite(PLUG_1, LOW);
  pinMode(PLUG_2, OUTPUT);
  digitalWrite(PLUG_2, LOW);
  pinMode(PLUG_3, OUTPUT);
  digitalWrite(PLUG_3, LOW);
  pinMode(PLUG_4, OUTPUT);
  digitalWrite(PLUG_4, LOW);

  // Get the mac address of the ESP module
  WiFi.macAddress(MAC_array);
  for (int i = 0; i < sizeof(MAC_array); ++i) {
    sprintf(MAC_char, "%s%02X:", MAC_char, MAC_array[i]);
  }
  MAC_char[strlen(MAC_char) - 1] = '\0';
  Serial.print("Printing MAC: ");
  Serial.print(MAC_char);
  Serial.println();
}

void BroadcastSetup(void) {
  Udp.begin(BROADCAST_PORT);
  Udp.begin(NOTIFICATION_PORT);
}

bool SSIDSearch(void) {
  Serial.println("Start SSID search from EEPROM");
  // Read EEPROM for SSID and Password
  Serial.println("Reading SSID from EEPROM");
  String essid;
  for (int i = 0; i < 32; ++i) {
    essid += char(EEPROM.read(i));
  }
  Serial.print("SSID: ");
  Serial.println(essid);
  Serial.println("Reading Password from EEPROM");
  String epass = "";
  for (int i = 32; i < 96; ++i) {
    epass += char(EEPROM.read(i));
  }
  Serial.print("PASS: ");
  Serial.println(epass);
  if ( essid.length() > 1 ) {
    // Try to connect to AP with given SSID and Password
    WiFi.begin(essid.c_str(), epass.c_str());
    if ( TestWifi() ) {
      Serial.println("Wifi test successful");
      return true;
    }
    else {
      Serial.println("Wifi test failed");
      return false;
    }
  }
}

void ZoneSearch(void) {
  Serial.println("Start Zone search from EEPROM");
  // Read EEPROM for SSID and Password
  Serial.println("Reading Zone from EEPROM");
  for (int i = 100; i < 116; ++i) {
    zone += char(EEPROM.read(i));
  }
  Serial.print("Zone: ");
  Serial.println(zone);

  Serial.println("Reading Appliance Type from EEPROM");
  for (int i = 116; i < 132; ++i) {
    appl_type += char(EEPROM.read(i));
  }
  Serial.print("Appliance Type: ");
  Serial.println(appl_type);

  Serial.println("Reading Appliance Name from EEPROM");
  for (int i = 132; i < 148; ++i) {
    appl_name += char(EEPROM.read(i));
  }
  Serial.print("Appliance Name: ");
  Serial.println(appl_name);
}

bool TestWifi(void) {
  int retries = 0;
  Serial.println("Waiting for Wi-Fi to connect");
  while ( retries < MAX_RETRIES ) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected");
      return true;
    }
    delay(500);
    Serial.print(WiFi.status());
    retries++;
  }
  Serial.println("Connect timed out");
  return false;
}

void WebServiceInit(void) {
  Serial.println("");
  Serial.println("Initializing Web Services");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.softAPIP());

  // Starting web server
  server.begin();
  Serial.println("Server started");

  // Starting mDNS Service
  MDNSService();
}

void MDNSService(void) {
  // Set up mDNS responder:
  // - first argument is the domain name, in this example
  //   the fully-qualified domain name is "esp8266.local"
  // - second argument is the IP address to advertise
  //   we send our IP address on the WiFi network
  if (!MDNS.begin("esp8266", WiFi.localIP())) {
    Serial.println("Error setting up MDNS responder!");
    return;
  }
  Serial.println("mDNS responder started");

  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
  return;
}

void WebServiceDaemon(bool webtype) {
  while (1) {
    // Running Web Service
    WebService(webtype);
    if (reboot_flag) {
      delay(10000);
      ESP.restart();
    }
    // reboot the device in AP mode if no configuration is done in 10 mins
    if (count % 12000 == 0 && webtype) {
      reboot_flag = 1;
    }
  }
}

void WebService(bool webtype) {
  // Match the request
  int which_plug = -1; // Selects the plug to use
  int state = -1; // Initial state

  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    if (count % 100 == 0) {
      Broadcast();
    }
    delay(50);
    count++;
    return;
  }
  Serial.println("");
  Serial.println("New client");

  // Wait for data from client to become available
  while (client.connected() && !client.available()) {
    delay(1);
  }

  // Read the first line of HTTP request
  String req = client.readStringUntil('\r');

  // First line of HTTP request looks like "GET /path HTTP/1.1"
  // Retrieve the "/path" part by finding the spaces
  int addr_start = req.indexOf(' ');
  int addr_end = req.indexOf(' ', addr_start + 1);
  if (addr_start == -1 || addr_end == -1) {
    Serial.print("Invalid request: ");
    Serial.println(req);
    return;
  }
  req = req.substring(addr_start + 1, addr_end);
  Serial.print("Request: ");
  Serial.println(req);
  client.flush();
  String s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>";
  if (webtype) {
    if (req == "/")
    {
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      s += "Hello from thingSocket at ";
      s += ipStr;
      s += "<p>";
      s += st;
      s += "<form method='get' action='a'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>";

      Serial.println("Sending 200");
    }
    else if ( req.startsWith("/a?ssid=") ) {
      // /a?ssid=blahhhh&pass=poooo
      Serial.println("clearing eeprom");
      for (int i = 0; i < 96; ++i) {
        EEPROM.write(i, 0);
      }
      String qssid;
      qssid = req.substring(8, req.indexOf('&'));
      char bssid[32];
      qssid.toCharArray(bssid, qssid.length() + 1);
      urlDecode(bssid);
      qssid = String(bssid);
      Serial.println(qssid);
      Serial.println("");
      String qpass;
      qpass = req.substring(req.lastIndexOf('=') + 1);
      char bpass[64];
      qpass.toCharArray(bpass, qpass.length() + 1);
      urlDecode(bpass);
      qpass = String(bpass);
      Serial.println(qpass);
      Serial.println("");

      Serial.println("writing eeprom ssid:");
      for (int i = 0; i < qssid.length(); ++i) {
        EEPROM.write(i, qssid[i]);
        Serial.print("Wrote: ");
        Serial.println(qssid[i]);
      }
      Serial.println("writing eeprom pass:");
      for (int i = 0; i < qpass.length(); ++i) {
        EEPROM.write(32 + i, qpass[i]);
        Serial.print("Wrote: ");
        Serial.println(qpass[i]);
      }
      EEPROM.commit();
      s += "Hello from thingSocket ";
      s += "Found ";
      s += req;
      s += "<p> saved to EEPROM... System will reboot in 10 seconds";
      reboot_flag = true;
    }
    else {
      s = "HTTP/1.1 404 Not Found\r\n\r\n<!DOCTYPE HTML>\r\n<html>";
      s += "<h1>404</h1><h3>Page Not Found</h3>";
      Serial.println("Sending 404");
    }
  }
  else {
    if (req == "/")
    {
      s += "Hello from thingSocket";
      s += "<p>";
      Serial.println("Sending 200");
    }
    else if (req.startsWith("/plug")) {
      if (req == "/plug/read") {
        state = -2;
      }
      else if (req == "/plug/1/0") {
        which_plug = PLUG_1;
        state = 0;
      }
      else if (req == "/plug/1/1") {
        which_plug = PLUG_1;
        state = 1;
      }
      else if (req == "/plug/2/0") {
        which_plug = PLUG_2;
        state = 0;
      }
      else if (req == "/plug/2/1") {
        which_plug = PLUG_2;
        state = 1;
      }
      else if (req == "/plug/3/0") {
        which_plug = PLUG_3;
        state = 0;
      }
      else if (req == "/plug/3/1") {
        which_plug = PLUG_3;
        state = 1;
      }
      else if (req == "/plug/4/0") {
        which_plug = PLUG_4;
        state = 0;
      }
      else if (req == "/plug/4/1") {
        which_plug = PLUG_4;
        state = 1;
      }

      // Set the plugs according to the request
      if (state >= 0) {
        digitalWrite(which_plug, state);
        NotificationBroadcast(which_plug, state);
      }

      // Prepare the response
      if (state >= 0) {
        s += "PLUG ";
        s += String(which_plug);
        s += " is now ";
        s += (state > 0) ? "ON" : "OFF";
      }
      else if (state == -2) {
        s += "Plug ";
        s += PLUG_1;
        s += " = ";
        s += String(digitalRead(PLUG_1));
        s += "<br>"; // Go to the next line.
        s += "Plug ";
        s += PLUG_2;
        s += " = ";
        s += String(digitalRead(PLUG_2));
        s += "<br>"; // Go to the next line.
        s += "Plug ";
        s += PLUG_3;
        s += " = ";
        s += String(digitalRead(PLUG_3));
        s += "<br>"; // Go to the next line.
        s += "Plug ";
        s += PLUG_4;
        s += " = ";
        s += String(digitalRead(PLUG_4));
      }
      else {
        s += "Invalid Request.<br> Try /plug/<1to4>/<0or1>, or /plug/read.";
      }
    }
    else if (req == "/setappliance")
    {
      s += "Hello from thingSocket </br>";
      s += "Please fill";
      s += "<form method='get' action='appl'><label>Zone: </label><input name='zone' length=15><label>Appliance Type: </label><input name='appl_type' length=15><label>Appliance Name: </lable><input name='appl_name' length=15><input type='submit'></form>";

      Serial.println("Sending 200");
    }
    else if ( req.startsWith("/appl?zone=") ) {
      // /appl?zone=hall&appl_type=bulb&appl_name=user-name
      Serial.println("clearing eeprom");
      for (int i = 100; i < 150; ++i) {
        EEPROM.write(i, 0);
      }
      zone = req.substring(11, req.indexOf('&'));
      char qzone[16];
      zone.toCharArray(qzone, zone.length() + 1);
      urlDecode(qzone);
      zone = String(qzone);
      Serial.println(zone);
      Serial.println("");

      appl_type = req.substring(req.indexOf('&') + 1, req.lastIndexOf('&'));
      appl_type = appl_type.substring(req.indexOf('='));
      char qappl_type[16];
      appl_type.toCharArray(qappl_type, appl_type.length() + 1);
      urlDecode(qappl_type);
      appl_type = String(qappl_type);
      Serial.println(appl_type);
      Serial.println("");

      appl_name = req.substring(req.lastIndexOf('=') + 1);
      char qappl_name[16];
      appl_name.toCharArray(qappl_name, appl_name.length() + 1);
      urlDecode(qappl_name);
      appl_name = String(qappl_name);
      Serial.println(appl_name);
      Serial.println("");

      Serial.println("writing eeprom Zone:");
      for (int i = 0; i < zone.length(); ++i) {
        EEPROM.write(100 + i, zone[i]);
        Serial.print("Wrote: ");
        Serial.println(zone[i]);
      }
      Serial.println("writing eeprom Appl_type:");
      for (int i = 0; i < appl_type.length(); ++i) {
        EEPROM.write(116 + i, appl_type[i]);
        Serial.print("Wrote: ");
        Serial.println(appl_type[i]);
      }
      Serial.println("writing eeprom Appl_name:");
      for (int i = 0; i < appl_name.length(); ++i) {
        EEPROM.write(132 + i, appl_name[i]);
        Serial.print("Wrote: ");
        Serial.println(appl_name[i]);
      }
      EEPROM.commit();
      s += "Hello from thingSocket ";
      s += "Found ";
      s += req;
      s += "<p> saved to EEPROM...";
    }
    else if ( req.startsWith("/factoryreset") ) {
      s += "Hello from thingSocket";
      s += "<p>Factory Resetting the device<p>";
      Serial.println("Sending 200");
      Serial.println("clearing eeprom");
      for (int i = 0; i < 155; ++i) {
        EEPROM.write(i, 0);
      }
      String qzone = "default";
      Serial.println("writing eeprom Zone with default value");
      for (int i = 0; i < zone.length(); ++i) {
        EEPROM.write(100 + i, qzone[i]);
        Serial.print("Wrote: ");
        Serial.println(qzone[i]);
      }
      String qappl_type = "default";
      Serial.println("writing eeprom Appl_type with default value");
      for (int i = 0; i < appl_type.length(); ++i) {
        EEPROM.write(116 + i, qappl_type[i]);
        Serial.print("Wrote: ");
        Serial.println(qappl_type[i]);
      }
      String qappl_name = "default";
      Serial.println("writing eeprom Appl_name:");
      for (int i = 0; i < appl_name.length(); ++i) {
        EEPROM.write(132 + i, qappl_name[i]);
        Serial.print("Wrote: ");
        Serial.println(qappl_name[i]);
      }
      EEPROM.commit();
      reboot_flag = true;
    }
    else if ( req.startsWith("/reboot")) {
      s += "Rebooting thingSocket";
      Serial.println("Sending 200");
      reboot_flag = true;
    }
    else
    {
      s = "HTTP/1.1 404 Not Found\r\n\r\n<!DOCTYPE HTML>\r\n<html>";
      s += "<h1>404</h1>Page Not Found";
      Serial.println("Sending 404");
    }
  }
  s += "</html>\r\n\r\n";
  client.print(s);
  Serial.println("Client disconnected");

  // The client will actually be disconnected
  // when the function returns and 'client' object is detroyed
  return;
}

void SetupAP(void) {

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");
  st = "<ul>";
  for (int i = 0; i < n; ++i)
  {
    // Print SSID and RSSI for each network found
    st += "<li>";
    st += i + 1;
    st += ": ";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);
    st += ")";
    st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
    st += "</li>";
  }
  st += "</ul>";
  delay(100);
  WiFi.softAP(APssid);
  Serial.println("Initiating Soft AP");
  Serial.println("");
  WebServiceInit();
  WebServiceDaemon(1);
}

void Broadcast(void) {
  IPAddress ip = WiFi.localIP();
  String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  for (int i = 0; i < 3; i++) {
    bcast[i] = ip[i];
  }
  bcast[3] = 255;
  // Building up the Broadcast message
  Udp.beginPacket(bcast, BROADCAST_PORT);
  String brdcast_msg;
  brdcast_msg += "thingTronics|";
  brdcast_msg += "thingSocket|";
  brdcast_msg += hardware_version;
  brdcast_msg += ":";
  brdcast_msg += software_version;
  brdcast_msg += "|";
  brdcast_msg += zone;
  brdcast_msg += "|";
  brdcast_msg += appl_type;
  brdcast_msg += "|";
  brdcast_msg += appl_name;
  brdcast_msg += "|";
  //  brdcast_msg += MAC_char;
  //  brdcast_msg += "|";
  //  brdcast_msg += ipStr;
  //  brdcast_msg += "|";
  Serial.println(brdcast_msg);
  Udp.write(brdcast_msg.c_str());
  delay(10);
  Udp.endPacket();
}

void NotificationBroadcast(int which_plug, int state) {
  // Building up the Notification message
  Udp.beginPacket(bcast, NOTIFICATION_PORT);
  String notif_msg;
  notif_msg += "thingTronics|";
  notif_msg += "thingSocket|";
  notif_msg += hardware_version;
  notif_msg += ":";
  notif_msg += software_version;
  notif_msg += "|";
  notif_msg += zone;
  notif_msg += "|";
  notif_msg += appl_type;
  notif_msg += "|";
  notif_msg += appl_name;
  notif_msg += "|";
  //notif_msg += MAC_char;
  //notif_msg += "|";
  //notif_msg += ipStr;
  //notif_msg += "|";
  notif_msg += String(which_plug);
  notif_msg += "|";
  notif_msg += (state > 0) ? "ON|" : "OFF|";
  Serial.println(notif_msg);
  Udp.write(notif_msg.c_str());
  delay(10);
  Udp.endPacket();
}

/**
 * Perform URL percent decoding.
 * Decoding is done in-place and will modify the parameter.
 */
void urlDecode(char *src) {
  char *dst = src;
  while (*src) {
    if (*src == '+') {
      src++;
      *dst++ = ' ';
    }
    else if (*src == '%') {
      // handle percent escape
      *dst = '\0';
      src++;
      if (*src >= '0' && *src <= '9') {
        *dst = *src++ - '0';
      }
      else if (*src >= 'A' && *src <= 'F') {
        *dst = 10 + *src++ - 'A';
      }
      else if (*src >= 'a' && *src <= 'f') {
        *dst = 10 + *src++ - 'a';
      }
      // this will cause %4 to be decoded to ascii @, but %4 is invalid
      // and we can't be expected to decode it properly anyway
      *dst <<= 4;
      if (*src >= '0' && *src <= '9') {
        *dst |= *src++ - '0';
      }
      else if (*src >= 'A' && *src <= 'F') {
        *dst |= 10 + *src++ - 'A';
      }
      else if (*src >= 'a' && *src <= 'f') {
        *dst |= 10 + *src++ - 'a';
      }
      dst++;
    }
    else {
      *dst++ = *src++;
    }
  }
  *dst = '\0';
}
