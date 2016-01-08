/*
 *  This sketch is the source code for Wi-Plug.
 *  The sketch will search for SSID and Password in EEPROM and 
 *  tries to connect to the AP using the SSID and Password. 
 *  If it fails then it boots into AP mode and asks for SSID and Password from the user
 *  API for AP (SSID = Wi-Plug) 
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
 *    http://server_ip/cleareeprom will clear the EEPROM contents. Its serves the purpose of factory resetting the device.
 *    http://server_ip/reboot will reboot the device after 10 seconds
 *  server_ip is the IP address of the ESP8266 module, will be 
 *  printed to Serial when the module is connected.
 *  The cpmplete project can be cloned @ https://github.com/automote/smart-plug.git
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
#define MAX_RETRIES 20

MDNSResponder mdns;
// Create an instance of the Web server
// specify the port to listen on as an argument
WiFiServer server(80);

const char* ssid = "Wi-Plug";
String st;
uint8_t MAC_array[6];
char MAC_char[18];

static unsigned char bcast[4] = { 255, 255, 255, 255 } ;   // broadcast IP address
unsigned int count = 0;
bool reboot_flag = false;

// Create an instance of the UDP server
WiFiUDP Udp;

void setup() {
  bool AP_required = false;
  // Initialise the hardware
  InitHardware();

  // Setting up the broadcast service
  BroadcastSetup();
  
  // Search for SSID and password from the EEPROM first and try to connect to AP
  AP_required = !SSIDSearch();

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
  for (int i = 0; i < 32; ++i)
  {
    essid += char(EEPROM.read(i));
  }
  Serial.print("SSID: ");
  Serial.println(essid);
  Serial.println("Reading Password from EEPROM");
  String epass = "";
  for (int i = 32; i < 96; ++i)
  {
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

bool TestWifi(void) {
  int retries = 0;
  Serial.println("Waiting for Wi-Fi to connect");
  while ( retries < MAX_RETRIES ) {
    if (WiFi.status() == WL_CONNECTED) {
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
  if (!MDNS.begin("esp8266"),WiFi.localIP()) {
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
      s += "Hello from Wi-Plug at ";
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
      Serial.println(qssid);
      Serial.println("");
      String qpass;
      qpass = req.substring(req.lastIndexOf('=') + 1);
      Serial.println(qpass);
      Serial.println("");

      Serial.println("writing eeprom ssid:");
      for (int i = 0; i < qssid.length(); ++i)
      {
        EEPROM.write(i, qssid[i]);
        Serial.print("Wrote: ");
        Serial.println(qssid[i]);
      }
      Serial.println("writing eeprom pass:");
      for (int i = 0; i < qpass.length(); ++i)
      {
        EEPROM.write(32 + i, qpass[i]);
        Serial.print("Wrote: ");
        Serial.println(qpass[i]);
      }
      EEPROM.commit();
      s += "Hello from ESP8266 ";
      s += "Found ";
      s += req;
      s += "<p> saved to EEPROM... System will reboot in 10 seconds";
      reboot_flag = true;
    }
    else
    {
      s = "HTTP/1.1 404 Not Found\r\n\r\n<!DOCTYPE HTML>\r\n<html>";
      s += "<h1>404</h1><h3>Page Not Found</h3>";
      Serial.println("Sending 404");
    }
  }
  else {
    if (req == "/")
    {
      s += "Hello from Wi-Plug";
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

      if (DEBUG) {
        Serial.print("which plug is ");
        Serial.println(which_plug);
        Serial.print("State is ");
        Serial.println(state);
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
    else if ( req.startsWith("/cleareeprom") ) {
      s += "Hello from Wi-Plug";
      s += "<p>Clearing the EEPROM<p>";
      Serial.println("Sending 200");
      Serial.println("clearing eeprom");
      for (int i = 0; i < 96; ++i) {
        EEPROM.write(i, 0);
      }
      EEPROM.commit();
    }
    else if ( req.startsWith("/reboot")) {
      s += "Rebooting Wi-Plug";
      Serial.println("Sending 200");
      reboot_flag = true;
    }
    else
    {
      s = "HTTP/1.1 404 Not Found\r\n\r\n<!DOCTYPE HTML>\r\n<html>";
      s += "<h1>404</h1><h3>Page Not Found</h3>";
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
  WiFi.softAP(ssid);
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
  String brdcast_msg = "thingTronics|";
  brdcast_msg += "WiFiPlug|";
  brdcast_msg += "v0.5|";
  brdcast_msg += MAC_char;
  brdcast_msg += "|";
  brdcast_msg += ipStr;
  brdcast_msg += "|";
  Serial.println(brdcast_msg);
  Udp.write(brdcast_msg.c_str());
  Udp.endPacket();
}

void NotificationBroadcast(int which_plug, int state) {
  IPAddress ip = WiFi.localIP();
  String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);

  // Building up the Notification message
  Udp.beginPacket(bcast, NOTIFICATION_PORT);
  String notif_msg = "thingTronics|";
  notif_msg += "WiFiPlug|";
  notif_msg += "v0.5|";
  notif_msg += MAC_char;
  notif_msg += "|";
  notif_msg += ipStr;
  notif_msg += "|";
  notif_msg += String(which_plug);
  notif_msg += "|";
  notif_msg += (state > 0) ? "ON|" : "OFF|";
  Serial.println(notif_msg);
  Udp.write(notif_msg.c_str());
  Udp.endPacket();
}
