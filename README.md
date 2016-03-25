# thingSocket
__thingSocket__ is a Wi-Fi enabled socket/plug coupled with open APIs for people to develop, deploy or even clone.

__thingSocket__ is an attempt to promote IoT by providing open and hackable devices that you use everyday. Devices like bulbs, sockets, presence detectors and many more. The idea revolves around identifying frequently accessed devices and converting them into IoT devices by adding Wi-Fi, BLE or ZigBee capabilities.

We are starting with a Wi-Fi Socket and a Wi-Fi Bulb. These devices have the crowd favourite ESP8266 modules and we are planning to keep the hardware and software design open. This would encourage school and college students and even IoT enthusiasts to get into the IoT Bandwagon.

## Software
The software is completely written in __Arduino IDE__, so anyone familiar with the IDE can tinker with the code and personalise the device according to his taste.
The Algorithmic flow chart is mentioned below:
<p align="center">
<img width="60%" height="600px" src="https://github.com/automote/thingSocket/blob/master/flow-chart.png" />

The sketch will search for SSID and Password in EEPROM and tries to connect to the AP using the SSID and Password. If it fails then it boots into AP mode and asks for SSID and Password from the user. Connect to the thingSocket using the following: (SSID = thingSocket-<last 3 digits of MAC> and PASSWORD = 12345678)

While in __AP__ mode SSID and Password shoud be given using the following API. A webpage is also provided for entering SSID and Password if you are using the browser method.
- http://192.168.4.1/a?ssid="yourSSID"&pass="yourPSKkey"

The ESP in thingSocket acts as Webserver on port 80 and has the following __APIs__

The server will set a GPIO14 pin depending on the request
 - http://server_ip/plug/read will read all the plug status,
 - http://server_ip/plug/on will set the GPIO14 low,
 - http://server_ip/plug/off will set the GPIO14 high

Other functionality include
- http://server_ip/factoryreset will clear the EEPROM contents. Its serves the purpose of factory resetting the device
- http://server_ip/reboot will reboot the device after 10 seconds
- http://server_ip/setappliance will set the appliance location, type and name

where <I>server_ip</I> is the IP address of the ESP8266 module, will be printed to Serial when the module is connected.
- v0.5 is the standalone version
- v0.6 is currently developed version
- other sketches are the modules which were used in the v0.5 and v0.6
- v0.7 is in development which makes use of Wi-Fi Manager by <i>tzapu</i> https://github.com/tzapu/WiFiManager


## Hardware
The hardware is kept simple and has been designed in Eagle for anyone to use the schematic and board files to develop his own product. We are even planing to provide __GERBER__ files once we ready with the Design. 

- For Geeks, they can directly buy the hardware from us or can make their own device
- For Tinkerers, UART pins are provided to flash the code of their choice or improve upon the already existing code
- For ESP Extremists, they can flash NodeMCU firmware and start coding in Lua. Example codes will be provided soon

### Bugs
Code contains bugs some of which are critical while others are non critical
- Critical:
  - ~~while submitting the SSID and password the URL doesnot encode spacial characters~~
  - PLUG status is not stored in case of power cut
  - PLUG will remain off on restart
  - After reboot broadcast and multicast packets have decreased length
  - Watchdog timer setup to reset device if its in loop
   
- Non-Critical:
  - Previous SSID and password still remains in memory even after factory reset
  - ~~multicast packets are not broken into separate packets~~
  - ~~string of appl_type is not getting parsed properly~~
  - ~~AP mode remains on in STA mode as well~~

#### Inspiration
- http://www.esp8266.com/viewtopic.php?f=29&t=2520

#### Alpha Version
  - Alpha version is available for testing and verification
People who are interested can contact Lovelesh: lovelesh06101990@gmail.com