#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <EEPROM.h>

// D5 ~ 8 are smart plug GPIOs
#define PLUG_1 14
#define PLUG_2 12
#define PLUG_3 13
#define PLUG_4 15

#define BROADCAST_PORT 8888   // Port for sending general broadcast messages
#define NOTIFICATION_PORT 8000     // Port for notification broadcasts
// For debugging interface
#define DEBUG 0

MDNSResponder mdns;
// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);

const char* ssid = "Smart-Plug";
String st;
uint8_t MAC_array[6];
char MAC_char[18];

static unsigned char bcast[4] = { 192, 168, 0, 255 } ;   // broadcast IP address
unsigned char count = 0;

// Create an instance of the UDP server
WiFiUDP Udp;

void setup() {
  // Initialise the hardware
  InitHardware();

  //WiFiSetup();
  BroadcastSetup();
  // Search for SSID and password from the EEPROM first and try to connect to AP
  SSIDSearch();
  // If it fails then make yourself AP and ask for SSID and password from user
  SetupAP();
}

void loop() {
  // Serving the requests from the client
  ServeRequest(0);
}

void InitHardware() {
  Serial.begin(115200);
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
  Serial.print("Printing MAC: ");
  Serial.print(MAC_char);
}

//void WiFiSetup() {
//  // Connect to WiFi network
//  Serial.println();
//  Serial.println();
//  Serial.print("Connecting to ");
//  Serial.println(SSID);
//
//  WiFi.begin(SSID, PASSWORD);
//
//  while (WiFi.status() != WL_CONNECTED) {
//    delay(500);
//    Serial.print(".");
//  }
//  Serial.println("");
//  Serial.println("WiFi connected");
//}

void BroadcastSetup() {
  Udp.begin(BROADCAST_PORT);
  Udp.begin(NOTIFICATION_PORT);
}

void SSIDSearch() {
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
    if ( TestWifi() == 20 ) {
      Serial.println("starting testing wifi connection");
      LaunchWeb(0);
      return;
    }
  }
}

int TestWifi(void) {
  int c = 0;
  Serial.println("Waiting for Wifi to connect");
  while ( c < 20 ) {
    if (WiFi.status() == WL_CONNECTED) {
      return (20);
    }
    delay(500);
    Serial.print(WiFi.status());
    c++;
  }
  Serial.println("Connect timed out, opening AP");
  return (10);
}

void LaunchWeb(int webtype) {
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.softAPIP());

  //          if (!mdns.begin("esp8266",WiFi.localIP())) {
  //            Serial.println("Error setting up MDNS responder!");
  //            while(1) {
  //              delay(1000);
  //            }
  //          }
  //          Serial.println("mDNS responder started");
  // Start the server
  server.begin();
  Serial.println("Server started");
  int b = 20;
  int c = 0;
  while (b == 20) {
    b = MDNSService(webtype);
  }
  //server.close();
}

int MDNSService(int webtype) {
  // Check for any mDNS queries and send responses
  //  mdns.update();
  // Add service to MDNS-SD
  //MDNS.addService("http", "tcp", 80);

  return(ServeRequest(webtype));
}

int ServeRequest(int webtype) {
  // Match the request
  int which_plug = -1; // Selects the plug to use
  int state = -1; // Initial state
  
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    if (count % 100 == 0) {
      Broadcast();
      count == 0;
    }
    delay(50);
    count++;
    return (20);
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
    return (20);
  }
  req = req.substring(addr_start + 1, addr_end);
  Serial.print("Request: ");
  Serial.println(req);
  client.flush();
  String s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>";
  if ( webtype == 1 ) {
    if (req == "/")
    {
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      s += "Hello from ESP8266 at ";
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
    }
    else
    {
      s = "HTTP/1.1 404 Not Found\r\n\r\n";
      Serial.println("Sending 404");
    }
  }
  else
  {
    if (req == "/")
    {
      s += "Hello from ESP8266";
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
        Serial.println("which plug is " + which_plug);
        Serial.println("State is " + state);
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
    else
    {
      s = "HTTP/1.1 404 Not Found\r\n\r\n";
      Serial.println("Sending 404");
    }
  }
  s += "</html>\r\n\r\n";
  client.print(s);
  Serial.println("Client disconnected");

  // The client will actually be disconnected
  // when the function returns and 'client' object is detroyed
  return (20);
}

void SetupAP() {

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
  Serial.println("Soft AP");
  Serial.println("");
  LaunchWeb(1);
  Serial.println("over");
}

void Broadcast() {
  Udp.beginPacket(bcast, BROADCAST_PORT);
  String brdcast_msg = "thingTronics|";
  brdcast_msg += "WiFiPlug|";
  brdcast_msg += "v0.5|";
  brdcast_msg += MAC_char;
  brdcast_msg += "|";
  brdcast_msg += WiFi.localIP();
  brdcast_msg += "|";
  Serial.println(brdcast_msg);
  Udp.write(brdcast_msg.c_str());
  Udp.endPacket();
}

void NotificationBroadcast(int which_plug, int state) {
  Udp.beginPacket(bcast, NOTIFICATION_PORT);
  String notif_msg = "thingTronics|";
  notif_msg += "WiFiPlug|";
  notif_msg += "v0.5|";
  notif_msg += MAC_char;
  notif_msg += "|";
  notif_msg += WiFi.localIP();
  notif_msg += "|";
  notif_msg += String(which_plug);
  notif_msg += "|";
  notif_msg += (state > 0) ? "ON|" : "OFF|";
  Serial.println(notif_msg);
  Udp.write(notif_msg.c_str());
  Udp.endPacket();
}
