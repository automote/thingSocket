/*
 *  This sketch demonstrates how to set up a simple HTTP-like server.
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
 *  server_ip is the IP address of the ESP8266 module, will be 
 *  printed to Serial when the module is connected.
 */

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
// D5 ~ 8 are smart plug GPIOs
#define PLUG_1 14 
#define PLUG_2 12
#define PLUG_3 13
#define PLUG_4 15

#define BRDCAST_PORT 8888   // Port for sending general broadcast messages
#define NOTIF_PORT 8000     // Port for notification broadcasts
// For debugging interface
#define DEBUG 0

// Global Variables
const char* SSID = "IOT";
const char* PASSWORD = "hasiot@123";
uint8_t MAC_array[6];
char MAC_char[18];

static unsigned char bcast[4] = { 192, 168, 0, 255 } ;   // broadcast IP address
unsigned char count = 0;

WiFiUDP Udp;

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);

void setup() {
  
  initHardware();
  WiFISetup();
  BroadcastSetup();
  
  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.println(WiFi.localIP());
}

void loop() {
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    if (count%100 == 0){
      Broadcast();
      count == 0;  
    }
    delay(50);
    count++;
    return;
  }
  
  // Wait until the client sends some data
  Serial.println("new client");
  while(!client.available()){
    delay(1);
  }
  
  // Read the first line of the request
  String req = client.readStringUntil('\r');
  Serial.println(req);
  client.flush();
  
  // Match the request
  int which_plug = -1; // Selects the plug to use
  int state = -1; // Initial state
  if (req.indexOf("/plug/read") != -1){
    state = -2;
  }
  if (req.indexOf("/plug/1/0") != -1){
    which_plug = PLUG_1;
    state = 0;
  }
  else if (req.indexOf("/plug/1/1") != -1){
    which_plug = PLUG_1;
    state = 1;
  }
  else if (req.indexOf("/plug/2/0") != -1){
    which_plug = PLUG_2;
    state = 0;
  }
  else if (req.indexOf("/plug/2/1") != -1){
    which_plug = PLUG_2;
    state = 1;
  }
  else if (req.indexOf("/plug/3/0") != -1){
    which_plug = PLUG_3;
    state = 0;
  }
  else if (req.indexOf("/plug/3/1") != -1){
    which_plug = PLUG_3;
    state = 1;
  }
  else if (req.indexOf("/plug/4/0") != -1){
    which_plug = PLUG_4;
    state = 0;
  }
  else if (req.indexOf("/plug/4/1") != -1){
    which_plug = PLUG_4;
    state = 1;
  }
 
  // Set the plugs according to the request
  if (state >= 0){
    digitalWrite(which_plug, state);
    notification_broadcast(which_plug, state);
  }
    
  client.flush();
  
  if(DEBUG){
  Serial.println("which plug is " + which_plug);
  Serial.println("State is " + state);
  }
    
  // Prepare the response
  
  String s = "HTTP/1.1 200 OK\r\n";
  s += "Content-Type: text/html\r\n\r\n";
  s += "<!DOCTYPE HTML>\r\n<html>\r\n";
  if (state >= 0) {
    s += "PLUG ";
    s += String(which_plug);
    s += " is now ";
    s += (state > 0)?"ON":"OFF";
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
  s += "</html>\n";

  // Send the response to the client
  client.print(s);
  delay(1);
  if(DEBUG){
  Serial.println(s);
  delay(10);
  }
  Serial.println("Client disconnected");

  // The client will actually be disconnected 
  // when the function returns and 'client' object is detroyed
}

void initHardware() {
  Serial.begin(115200);
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
    for (int i = 0; i < sizeof(MAC_array); ++i){
      sprintf(MAC_char,"%s%02X:",MAC_char,MAC_array[i]);
    }
  Serial.println("Printing MAC: ");
  Serial.print(MAC_char);
}

void WiFISetup() {
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(SSID);
  
  WiFi.begin(SSID, PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Make the broadcast address from the IP address
  
}

void BroadcastSetup() {
  Udp.begin(BRDCAST_PORT);
  Udp.begin(NOTIF_PORT);  
}

void Broadcast() {
  Udp.beginPacket(bcast, BRDCAST_PORT);
  String brdcast_msg = "thingTronics|";
  brdcast_msg += "WiFiPlug|";
  brdcast_msg += "v0.5|";
  brdcast_msg += MAC_char;
  brdcast_msg += "|";
  brdcast_msg += WiFi.localIP();
  brdcast_msg += "|"; 
  Serial.println(brdcast_msg); 
  Udp.write("hello");
  Udp.endPacket();
}

void notification_broadcast(int which_plug, int state){
  Udp.beginPacket(bcast, NOTIF_PORT);
  String notif_msg = "thingTronics|";
  notif_msg += "WiFiPlug|";
  notif_msg += "v0.5|";
  notif_msg += MAC_char;
  notif_msg += "|";
  notif_msg += WiFi.localIP();
  notif_msg += "|";
  notif_msg += String(which_plug);
  notif_msg += "|";
  notif_msg += (state > 0)?"ON|":"OFF|";
  Serial.println(notif_msg); 
  Udp.write("Notification Service");
  Udp.endPacket();
}
