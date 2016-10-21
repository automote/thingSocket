/*
 *  This sketch is the source code for thngSocket.
 *  The sketch will search for SSID and Password in EEPROM and
 *  tries to connect to the AP using the SSID and Password.
 *  If it fails then it boots into AP mode and asks for SSID and Password from the user
 *  API for AP (SSID = thingSocket-<last 3 digits of MAC> and PASSWORD = 12345678)
 *    http://192.168.4.1/a?ssid="yourSSID"&pass="yourPSKkey"
 *  A webpage is also provided for entering SSID and Password if you are using the browser method.
 *  The server will set a GPIO14 pin depending on the request
 *    http://server_ip/resource will read all the plug status,
 *    http://server_ip/resource/set?res=0&val=0 will set the GPIO14 low,
 *    http://server_ip/resource/set?res=0&val=100 will set the GPIO14 high
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
 *  modified 11 Feb 2016
 *  by Lovelesh Patel
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <EEPROM.h>

// GPIO14 connected to Socket, GPIO5 to SWITCH, GPIO4 to SWITCH_LED
// GPIO16 is for status
#define PLUG 14
#define CONNECT 16
#define SWITCH_LED 4
#define SWITCH 5

#define BROADCAST_PORT 5000   // Port for sending general broadcast messages
#define NOTIFICATION_PORT 5002     // Port for notification broadcasts

// For debugging interface
#define DEBUG 1
#define MAX_RETRIES 20  // Max retries for checking wifi connection
#define MAX_RESOURCES 1

MDNSResponder mdns;
// Create an instance of the Web server
// specify the port to listen on as an argument
WiFiServer server(80);

// Global Constant
const char* company_name = "thingTronics";
const char* hardware_version = "v1.0";
const char* software_version = "v1.1";
const char APpsk[] = "12345678";
const int resource_number = 0;

// Global Variable
String st;
String hostName = "thingSocket-";
String zone, appl_type, appl_name;
String macID;
static unsigned char bcast[4] = { 255, 255, 255, 255 } ;   // broadcast IP address
unsigned int count = 0;
#ifdef DEBUG
volatile unsigned int num = 0;
#endif

// Reboot flag to reboot the device when necessary
bool reboot_flag = false;
bool configure_flag = false;

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
void UpdatePlugNLED(int plug, int value);
void SetupAP(void);
void Broadcast(void);
void NotificationBroadcast(int which_plug, int state);
void UrlDecode(char *src);
void pin_ISR(void);
void myDelay(int);

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

  // prepare PLUG to control the socket i.e. GPIO14
  pinMode(PLUG, OUTPUT);
  digitalWrite(PLUG, LOW);

  // prepare the CONNECT LED i.e. GPIO16
  pinMode(CONNECT, OUTPUT);
  digitalWrite(CONNECT, HIGH);

  // prepare the SWITCH and SWITCH_LED i.e. GPIO4 and GPIO5
  pinMode(SWITCH, INPUT_PULLUP);
  pinMode(SWITCH_LED, OUTPUT);
  digitalWrite(SWITCH_LED, digitalRead(PLUG));
  attachInterrupt(digitalPinToInterrupt(SWITCH), pin_ISR, CHANGE);
  
  // Set the thingSocket into STATION mode
  WiFi.mode(WIFI_STA);
  
  // Get the mac address of the ESP module
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  macID = String(mac[WL_MAC_ADDR_LENGTH - 3], HEX) +
				 String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  hostName += macID;
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
#ifdef DEBUG
  Serial.print("SSID: ");
  Serial.println(essid);
  Serial.println("Reading Password from EEPROM");
#endif

  String epass = "";
  for (int i = 32; i < 96; ++i) {
    epass += char(EEPROM.read(i));
  }
#ifdef DEBUG
  Serial.print("PASS: ");
  Serial.println(epass);
#endif

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
#ifdef DEBUG
  Serial.print("Zone: ");
  Serial.println(zone);
#endif

  Serial.println("Reading Appliance Type from EEPROM");
  for (int i = 116; i < 132; ++i) {
    appl_type += char(EEPROM.read(i));
  }
#ifdef DEBUG
  Serial.print("Appliance Type: ");
  Serial.println(appl_type);
#endif
  Serial.println("Reading Appliance Name from EEPROM");
  for (int i = 132; i < 148; ++i) {
    appl_name += char(EEPROM.read(i));
  }
#ifdef DEBUG
  Serial.print("Appliance Name: ");
  Serial.println(appl_name);
#endif
}

bool TestWifi(void) {
  int retries = 0;
  WiFi.mode(WIFI_STA);
  Serial.println("Waiting for Wi-Fi to connect");
  while ( retries < MAX_RETRIES ) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected");
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
#ifdef DEBUG 
 Serial.println("");
  Serial.println("Initializing Web Services");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.softAPIP());
#endif
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
  int hostName_len = hostName.length() + 1; 
  char hostNameChar[hostName_len];
  hostName.toCharArray(hostNameChar, hostName_len);
  if (!MDNS.begin(hostNameChar)) {
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
  int value = -1; // Initial state

  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    if (count % 300 == 0) {
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
#ifdef DEBUG  
  Serial.println(req);
#endif
    return;
  }
  req = req.substring(addr_start + 1, addr_end);
#ifdef DEBUG
  Serial.print("Request: ");
  Serial.println(req);
#endif
  client.flush();
  String s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>";
  if (webtype) {
    if (req == "/") {
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      s += "Hello from thingSocket at ";
      s += ipStr;
      s += "<p>";
      s += st;
      s += "<form method='get' action='a'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input name='devicepassword' length=8><input type='submit'></form>";

      Serial.println("Sending 200");
    }
    else if ( req.startsWith("/a?ssid=") ) {
      // /a?ssid="SSID"&pass="PSK Key"&devicepassword=1234567
      Serial.println("clearing eeprom");
      for (int i = 0; i < 96; ++i) {
        EEPROM.write(i, 0);
      }
      String qssid;
      qssid = req.substring(8, req.indexOf('&'));
      char bssid[32];
      qssid.toCharArray(bssid, qssid.length() + 1);
      UrlDecode(bssid);
      qssid = String(bssid);
#ifdef DEBUG      
	  Serial.println(qssid);
      Serial.println("");
#endif
	  String qpass;
	  qpass = req.substring(req.indexOf('&'), req.lastIndexOf('&'));
      qpass = qpass.substring(qpass.indexOf('=') + 1);
      char bpass[64];
      qpass.toCharArray(bpass, qpass.length() + 1);
      UrlDecode(bpass);
      qpass = String(bpass);
#ifdef DEBUG      
	  Serial.println(qpass);
      Serial.println("");
#endif
	  String qdevpass;
	  qdevpass = req.substring(req.lastIndexOf('=') + 1);
	  char bdevpass[8];
      qdevpass.toCharArray(bdevpass, qdevpass.length() + 1);
	  UrlDecode(bdevpass);
	  qdevpass = String(bdevpass);
#ifdef DEBUG	  
	  Serial.println(qdevpass);
	  Serial.println("");
#endif	  
      Serial.println("writing ssid to EEPROM");
      for (int i = 0; i < qssid.length(); ++i) {
        EEPROM.write(i, qssid[i]);
#ifdef DEBUG		
        Serial.print("Wrote: ");
        Serial.println(qssid[i]);
#endif		
      }
      Serial.println("writing eeprom pass:");
      for (int i = 0; i < qpass.length(); ++i) {
        EEPROM.write(32 + i, qpass[i]);
#ifdef DEBUG
        Serial.print("Wrote: ");
        Serial.println(qpass[i]);
#endif		
      }
      EEPROM.commit();
      s += "Hello from thingSocket ";
#ifdef DEBUG
      s += "Found ";
      s += req;
#endif	  
      s += "<p> Details saved to EEPROM... System will reboot in 10 seconds";
      reboot_flag = true;
    }
    else {
      s = "HTTP/1.1 404 Not Found\r\n\r\n<!DOCTYPE HTML>\r\n<html>";
      s += "<h1>404</h1><h3>Page Not Found</h3>";
      Serial.println("Sending 404");
    }
  }
  else {
    if (req == "/") {
      s += "Hello from thingSocket";
      s += "<p>";
      Serial.println("Sending 200");
    }
	else if (req.startsWith("/resource")) {
		s += "Hello from thingSocket </br>";
		s += "<form method='get' action='/resource/set'><label>Resource No: </label><input name='res' length=2><label>Value: </label><input name='val' length=3><input type='submit'></form>";
		value = -2;
		which_plug = PLUG;
	  
		if (req.startsWith("/resource/set?res=")) {
			//resource/set?res=0&val=100
			// No need to acquire resource argument as thingSocket has only 1 resource
			// By Default res = 0

			// Getting value of resource
			String val = req.substring(req.lastIndexOf('=') + 1);
			value = val.toInt();
		}

		// Prepare the response
		if (value >= 0 && value <= 100) {
			// Update the status of PLUG and SWITCH_LED
			UpdatePlugNLED(which_plug, value);
			s += "Resource 0";
			s += " = ";
			s += String((digitalRead(PLUG) > 0) ? 100 : 0);
			s += "<br>"; // Go to the next line.
		}
		else if (value == -2) {
			s += "Resource 0";
			s += " = ";
			value = (digitalRead(PLUG) > 0) ? 100 : 0;
			s += String(value);
			s += "<br>"; // Go to the next line.
#ifdef DEBUG
			Serial.println(value); // Debug the value
#endif
			// Sent the queried resource via notification
			NotificationBroadcast(which_plug, value);
		}
		else {
			s += "Invalid Request<br> Try /resource/set?res=0&val=0|100|-2";
		}
    }
    else if (req == "/setappliance") {
      s += "Hello from thingSocket </br>";
      s += "Please fill";
      s += "<form method='get' action='appl'><label>Resource No: </label><input name='res' length=2><label>Zone: </label><input name='zone' length=15><label>Appliance Type: </label><input name='appl_type' length=15><label>Appliance Name: </lable><input name='appl_name' length=15><input type='submit'></form>";

      Serial.println("Sending 200");
    }
    else if ( req.startsWith("/appl?res=")) {
      // /appl?res=0&zone=hall&appl_type=bulb&appl_name=user-name
      // Resource for thingSocket is always 0

      zone = req.substring(req.indexOf('&') + 1, req.lastIndexOf('&'));
      // zone=hall&appl_type=bulb
      appl_type = zone.substring(zone.lastIndexOf('=') + 1);
      zone = zone.substring(zone.indexOf('=') + 1, zone.indexOf('&'));
      char qzone[16];
      zone.toCharArray(qzone, zone.length() + 1);
      UrlDecode(qzone);
      zone = String(qzone);
#ifdef DEBUG
	  Serial.println(zone);
#endif        
      char qappl_type[16];
      appl_type.toCharArray(qappl_type, appl_type.length() + 1);
      UrlDecode(qappl_type);
      appl_type = String(qappl_type);
#ifdef DEBUG      
	  Serial.println(appl_type);
#endif
      appl_name = req.substring(req.lastIndexOf('=') + 1);
      char qappl_name[16];
      appl_name.toCharArray(qappl_name, appl_name.length() + 1);
      UrlDecode(qappl_name);
      Serial.println(appl_name);
#ifdef DEBUG      
	  appl_name = String(qappl_name);
#endif
      Serial.println("writing eeprom Zone:");
      for (int i = 0; i < zone.length(); ++i) {
        EEPROM.write(100 + i, zone[i]);
#ifdef DEBUG      
		Serial.print("Wrote: ");
        Serial.println(zone[i]);
#endif		
      }
      Serial.println("writing eeprom Appl_type:");
      for (int i = 0; i < appl_type.length(); ++i) {
        EEPROM.write(116 + i, appl_type[i]);
#ifdef DEBUG		
        Serial.print("Wrote: ");
        Serial.println(appl_type[i]);
#endif		
      }
      Serial.println("writing eeprom Appl_name:");
      for (int i = 0; i < appl_name.length(); ++i) {
        EEPROM.write(132 + i, appl_name[i]);
#ifdef DEBUG		
        Serial.print("Wrote: ");
        Serial.println(appl_name[i]);
#endif		
      }
      EEPROM.commit();
      s += "Hello from thingSocket ";
#ifdef DEBUG	  
      s += "Found ";
      s += req;
#endif	  
      s += "<p> Details saved to EEPROM...";
      configure_flag = true;
      NotificationBroadcast(which_plug,value);
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
#ifdef DEBUG      
		Serial.print("Wrote: ");
        Serial.println(qzone[i]);
#endif		
      }
      String qappl_type = "default";
      Serial.println("writing eeprom Appl_type with default value");
      for (int i = 0; i < appl_type.length(); ++i) {
        EEPROM.write(116 + i, qappl_type[i]);
#ifdef DEBUG		
        Serial.print("Wrote: ");
        Serial.println(qappl_type[i]);
#endif		
      }
      String qappl_name = "default";
      Serial.println("writing eeprom Appl_name with default value");
      for (int i = 0; i < appl_name.length(); ++i) {
        EEPROM.write(132 + i, qappl_name[i]);
#ifdef DEBUG		
        Serial.print("Wrote: ");
        Serial.println(qappl_name[i]);
#endif		
      }
      EEPROM.commit();
      reboot_flag = true;
    }
    else if ( req.startsWith("/reboot")) {
      s += "Rebooting thingSocket";
      Serial.println("Sending 200");
      reboot_flag = true;
    }
    else {
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
  else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
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
  for (int i = 0; i < n; ++i) {
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
  char APssid[hostName.length() + 1];
  memset(APssid, 0, hostName.length() + 1);
  for (int i = 0; i < hostName.length(); i++) {
	APssid[i] = hostName.charAt(i);  
  }
  WiFi.softAP(APssid, APpsk);
  Serial.println("Initiating Soft AP");
  Serial.println("");
  WebServiceInit();
  WebServiceDaemon(1);
}

void UpdatePlugNLED(int plug, int value) {
  if (value != 0){
	  value = 100;
  }
  digitalWrite(SWITCH_LED, value);
  digitalWrite(plug, value);
  NotificationBroadcast(plug, value);
}

void Broadcast(void) {
  // Set the broadcast address
  IPAddress ip = WiFi.localIP();
  for (int i = 0; i < 3; i++) {
    bcast[i] = ip[i];
  }
  bcast[3] = 255;
  
  // thingTronics|thingSocket-ABCDEF|v0.1:v1.1|totalResources|
  // Building up the Broadcast message
  Udp.beginPacket(bcast, BROADCAST_PORT);
  String brdcast_msg;
  brdcast_msg += company_name;
  brdcast_msg += "|";
  brdcast_msg += hostName;
  brdcast_msg += "|";
  brdcast_msg += hardware_version;
  brdcast_msg += ":";
  brdcast_msg += software_version;
  brdcast_msg += "|";
  brdcast_msg += String(MAX_RESOURCES);
  brdcast_msg += "|";
  Udp.write(brdcast_msg.c_str());
  Udp.endPacket();
#ifdef DEBUG
  Serial.println(brdcast_msg);
#endif  
}

void NotificationBroadcast(int which_plug, int state) {
  //thingTronics|thingSocket-ABCDEF|zone|type|name|magicResourceNumber|value|
  // Building up the Notification message
  Udp.beginPacket(bcast, NOTIFICATION_PORT);
  String notif_msg;
  notif_msg += company_name;
  notif_msg += "|";
  notif_msg += hostName;
  notif_msg += "|";
  notif_msg += zone;
  notif_msg += "|";
  notif_msg += appl_type;
  notif_msg += "|";
  notif_msg += appl_name;
  notif_msg += "|";
  notif_msg += String(resource_number);
  notif_msg += "|";
  notif_msg += String((state > 0) ? "100|" : "0|");
  if(configure_flag) {
    notif_msg += "CONFIGURED|";
    configure_flag = false;
  }
  Udp.write(notif_msg.c_str());
  Udp.endPacket();
#ifdef DEBUG  
  Serial.println(notif_msg);
#endif  
}

/**
 * Perform URL percent decoding.
 * Decoding is done in-place and will modify the parameter.
 */
void UrlDecode(char *src) {
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

void pin_ISR() {
  detachInterrupt(digitalPinToInterrupt(SWITCH));
#ifdef DEBUG  
  String s;
  s += "ISR Called ";
  s += String(num);
  Serial.println(s);
  num++;
#endif
  
  myDelay(250);
  volatile int buttonState = digitalRead(SWITCH);
  UpdatePlugNLED(PLUG, !buttonState);
  attachInterrupt(digitalPinToInterrupt(SWITCH), pin_ISR, CHANGE);
}

void myDelay(int x) {
  for(int i=0; i<=x; i++) {
    delayMicroseconds(1000);
  }
}