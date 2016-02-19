/*
    This sketch is the source code for thingSocket.
    The sketch will search for SSID and Password in EEPROM and
    tries to connect to the AP using the SSID and Password.
    If it fails then it boots into AP mode and asks for SSID and Password from the user
    API for AP (SSID = thingSocket)
      http://192.168.4.1/a?ssid="yourSSID"&pass="yourPSKkey"
    A webpage is also provided for entering SSID and Password if you are using the browser method.
    The server will set a GPIO14 pin depending on the request
      http://server_ip/socket/read will read all the plug status,
      http://server_ip/socket/on will set the GPIO14 low,
      http://server_ip/socket/off will set the GPIO14 high
      http://server_ip/factoryreset will clear the EEPROM contents. Its serves the purpose of factory resetting the device.
      http://server_ip/reboot will reboot the device after 10 seconds
      http://server_ip/setappliance will set the appliance location, type and name
    server_ip is the IP address of the ESP8266 module, will be
    printed to Serial when the module is connected.
    A switch with LED is provided to display the status of the socket
    The complete project can be cloned @ https://github.com/automote/thingSocket.git

    Inspired by:
    https://github.com/chriscook8/esp-arduino-apboot
    This example code is under GPL v3.
    modified 11 Feb 2016
    by Lovelesh Patel
*/

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

// GPIO14 connected to Socket, GPIO5 to Switch, GPIO4 to SWITCH_LED
// GPIO16 is for Wi-Fi status
#define SOCKET 14
#define CONNECT 16
#define SWITCH_LED 4
#define SWITCH 5

#define BROADCAST_PORT 5000   // Port for sending general broadcast messages
#define NOTIFICATION_PORT 5002     // Port for notification broadcasts

// For debugging interface
#define DEBUG 1
#define MAX_RETRIES 20  // Max retries for checking wifi connection
#define MAGIC_BYTE 251

// Global Constant
const char* APssid = "thingSocket";
const char* hardware_version = "v1.0";
const char* software_version = "v1.0";
const int restartDelay = 3; //minimal time for button press to reset in sec
const int humanpressDelay = 50; // the delay in ms untill the press should be handled as a normal push by human. Button debouce. !!! Needs to be less than restartDelay & resetDelay!!!
const int resetDelay = 20; //Minimal time for button press to reset all settings and boot to config mode in sec

// Global Variables
String st;
String essid = "";
String epass = "";
String ipString;
String macString;
String netmaskString;
String gatewayString;
String devicename = "thingsocket";
String zone = "default";
String appl_type = "default";
String appl_name = "default";
uint8_t MAC_array[6];
char MAC_char[18];
char packetBuffer[255]; // buffer for holding incoming packets
static unsigned char bcast[4] = { 255, 255, 255, 255 } ;   // broadcast IP address
unsigned int count = 0;
volatile unsigned int num = 0;
int webMode; //decides if we are in setup, normal or local only mode
unsigned long button_count = 0; //Button press time counter

// Reboot flag to reboot the device when necessary
bool reboot_flag = false;

MDNSResponder mdns;
// Create an instance of the Web server
// specify the port to listen on as an argument
ESP8266WebServer server(80);

// DNS server
DNSServer dnsServer;

// Web based OTA update server
ESP8266HTTPUpdateServer httpUpdater;

IPAddress apIP(192, 168, 1, 1);
IPAddress netMsk(255, 255, 255, 0);

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
void UpdateSocketNLED(int which_socket, int state);
void SetupAP(void);
void Broadcast(void);
void NotificationBroadcast(int which_socket, int state);
void UrlDecode(char *src);
void pin_ISR(void);
void myDelay(int);
void CheckInitConfig(void);

void setup() {
  bool AP_required = false;
  // Initialise the hardware
  InitHardware();

  // Setting up the broadcast service
  BroadcastSetup();

  // Initialise the webserver for direct initialization
  WebServiceInit();

  // check if the device has been booted for the first time
  CheckInitConfig();

  // Search for SSID and password from the EEPROM first and try to connect to AP
  AP_required = !SSIDSearch();
  ZoneSearch();

  // If it fails then make yourself AP and ask for SSID and password from user
  if (AP_required) {
    SetupAP();
  }

}

void loop() {
  if (count % 1200 == 0) {
    // Test WiFi connection every minute and set the reboot flag if necessary
    // count is incremented roughly every 50ms
    Serial.println("checking wifi connection");
    reboot_flag = !TestWifi();
  }

  // Checks the status if the switch


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

  // prepare SOCKET to control the socket i.e. GPIO14
  pinMode(SOCKET, OUTPUT);
  digitalWrite(SOCKET, LOW);

  // prepare the CONNECT LED i.e. GPIO16
  pinMode(CONNECT, OUTPUT);
  digitalWrite(CONNECT, HIGH);

  // prepare the SWITCH and SWITCH_LED i.e. GPIO4 and GPIO5
  pinMode(SWITCH, INPUT);
  pinMode(SWITCH_LED, OUTPUT);
  digitalWrite(SWITCH_LED, HIGH);
  attachInterrupt(digitalPinToInterrupt(SWITCH), pin_ISR, CHANGE);

  // Set the thingSocket into STATION mode
  WiFi.mode(WIFI_STA);

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
  for (int i = 0; i < 32; ++i) {
    essid += char(EEPROM.read(i));
  }
  Serial.print("SSID: ");
  Serial.println(essid);
  
  Serial.println("Reading Password from EEPROM");
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
  WiFi.mode(WIFI_STA);
  Serial.println("Waiting for Wi-Fi to connect");
  while ( retries < MAX_RETRIES ) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected");
      Serial.println(WiFi.localIP());
      digitalWrite(CONNECT, LOW);
      return true;
    }
    delay(500);
    Serial.print(WiFi.status());
    retries++;
  }
  Serial.println("Connect timed out");
  digitalWrite(CONNECT, HIGH);
  return false;
}

void WebServiceInit(void) {
  Serial.println("");
  Serial.println("Initializing Web Services");

  // setting up the OTA update server
  httpUpdater.setup(&server);

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
  if (!MDNS.begin("thingSocket", WiFi.localIP())) {
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
  int which_socket = -1; // Selects the SOCKETS to use
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

      essid = req.substring(8, req.indexOf('&'));
      char bssid[32];
      essid.toCharArray(bssid, essid.length() + 1);
      UrlDecode(bssid);
      essid = String(bssid);
      Serial.println(essid);
      Serial.println("");
      
      epass = req.substring(req.lastIndexOf('=') + 1);
      char bpass[64];
      epass.toCharArray(bpass, epass.length() + 1);
      UrlDecode(bpass);
      epass = String(bpass);
      Serial.println(qpass);
      Serial.println("");

      Serial.println("writing eeprom ssid:");
      for (int i = 0; i < essid.length(); ++i) {
        EEPROM.write(i, essid[i]);
        Serial.print("Wrote: ");
        Serial.println(essid[i]);
      }
      Serial.println("writing eeprom pass:");
      for (int i = 0; i < epass.length(); ++i) {
        EEPROM.write(32 + i, epass[i]);
        Serial.print("Wrote: ");
        Serial.println(epass[i]);
      }
      delay(10);
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
    else if (req.startsWith("/socket")) {
      if (req == "/socket/read") {
        state = -2;
      }
      else if (req == "/socket/off") {
        which_socket = SOCKET;
        state = 0;
      }
      else if (req == "/socket/on") {
        which_socket = SOCKET;
        state = 1;
      }

      // Set the sockets according to the request
      if (state >= 0) {
        // Update the status of SOCKET and SWITCH_LED
        UpdateSocketNLED(which_socket, state);
      }

      // Prepare the response
      if (state >= 0) {
        s += "SOCKET ";
        //s += String(which_socket);
        s += " is now ";
        s += (state > 0) ? "ON" : "OFF";
      }
      else if (state == -2) {
        s += "Socket ";
        //s += SOCKET;
        s += " = ";
        s += String(digitalRead(SOCKET));
      }
      else {
        s += "Invalid Request.<br> Try /socket/<0or1>, or /socket/read.";
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
      char bzone[16];
      zone.toCharArray(bzone, zone.length() + 1);
      UrlDecode(bzone);
      zone = String(bzone);
      Serial.println(zone);
      Serial.println("");

      appl_type = req.substring(req.indexOf('&') + 1, req.lastIndexOf('&'));
      appl_type = appl_type.substring(req.indexOf('='));
      char bappl_type[16];
      appl_type.toCharArray(bappl_type, appl_type.length() + 1);
      UrlDecode(bappl_type);
      appl_type = String(bappl_type);
      Serial.println(appl_type);
      Serial.println("");

      appl_name = req.substring(req.lastIndexOf('=') + 1);
      char bappl_name[16];
      appl_name.toCharArray(bappl_name, appl_name.length() + 1);
      UrlDecode(bappl_name);
      appl_name = String(bappl_name);
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
      delay(10);
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
      zone = "default";
      Serial.println("writing eeprom Zone with default value");
      for (int i = 0; i < zone.length(); ++i) {
        EEPROM.write(100 + i, qzone[i]);
        Serial.print("Wrote: ");
        Serial.println(zone[i]);
      }
      appl_type = "default";
      Serial.println("writing eeprom Appl_type with default value");
      for (int i = 0; i < appl_type.length(); ++i) {
        EEPROM.write(116 + i, appl_type[i]);
        Serial.print("Wrote: ");
        Serial.println(appl_type[i]);
      }
      appl_name = "default";
      Serial.println("writing eeprom Appl_name with dafault value");
      for (int i = 0; i < appl_name.length(); ++i) {
        EEPROM.write(132 + i, appl_name[i]);
        Serial.print("Wrote: ");
        Serial.println(appl_name[i]);
      }
      delay(10);
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
  Serial.println(WiFi.softAPIP());
  WebServiceDaemon(1);
}

void UpdateSocketNLED(int which_socket, int state)
{
  digitalWrite(SWITCH_LED, !state);
  digitalWrite(which_socket, state);
  NotificationBroadcast(which_socket, state);
}

void Broadcast(void) {
  IPAddress ip = WiFi.localIP();
  //    String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
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
  Udp.endPacket();
}

void NotificationBroadcast(int which_socket, int state) {
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
  notif_msg += String(which_socket);
  notif_msg += "|";
  notif_msg += (state > 0) ? "ON|" : "OFF|";
  Serial.println(notif_msg);
  Udp.write(notif_msg.c_str());
  Udp.endPacket();
}

/**
   Perform URL percent decoding.
   Decoding is done in-place and will modify the parameter.
*/
void UrlDecode(char *src)
{
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

void pin_ISR()
{
  detachInterrupt(digitalPinToInterrupt(SWITCH));
  String s;
  s += "ISR Called ";
  s += String(num);
  Serial.println(s);
  num++;
  myDelay(250);
  volatile int buttonState = digitalRead(SWITCH);
  UpdateSocketNLED(SOCKET, buttonState);
  attachInterrupt(digitalPinToInterrupt(SWITCH), pin_ISR, CHANGE);
}

void myDelay(int x)
{
  for (int i = 0; i <= x; i++) {
    delayMicroseconds(1000);
  }
}

void CheckInitConfig(void)
{
  if (EEPROM.read(MAGIC_BYTE) != 786) {
    // if not load the default settings to EEPROM
    Serial.println("writing eeprom Zone with default value");
    for (int i = 0; i < zone.length(); ++i) {
        EEPROM.write(100 + i, zone[i]);
        Serial.print("Wrote: ");
        Serial.println(zone[i]);
    }
    Serial.println("writing eeprom Appl_type with default value");
    for (int i = 0; i < appl_type.length(); ++i) {
        EEPROM.write(116 + i, appl_type[i]);
        Serial.print("Wrote: ");
        Serial.println(appl_type[i]);
    }
    Serial.println("writing eeprom Appl_name with default value");
    for (int i = 0; i < appl_name.length(); ++i) {
        EEPROM.write(132 + i, appl_name[i]);
        Serial.print("Wrote: ");
        Serial.println(appl_name[i]);
    }
    Serial.println("writing the magic byte");
    EEPROM.write(MAGIC_BYTE, 786);
    delay(10);
    EEPROM.commit();
    delay(500);
  }
}

