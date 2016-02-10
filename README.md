# thingSocket
<p>
<b>thingSocket</b> is a Wi-Fi enabled socket/plug coupled with open APIs for people to develop, deploy or even clone.

<b>thingSocket</b> is an attempt to promote IoT by providing open and hackable devices that you use everyday. Devices like bulbs, sockets, presence detectors and many more. The idea revolves around identifying frequently accessed devices and converting them into IoT devices by adding Wi-Fi, BLE or ZigBee capabilities.

We are starting with a Wi-Fi Socket and a Wi-Fi Bulb. These devices have the crowd favourite ESP8266 modules and we are planning to keep the hardware and software design open. This would encourage school and college students and even IoT enthusiasts to get into the IoT Bandwagon.
</p>

## Software
<p>
The software is completely written in <b>Arduino IDE</b>, so anyone familiar with the IDE can tinker with the code and personalise the device according to his taste.
The Algorithmic flow chart is mentioned below:
<p align="center">
<img width="60%" height="600px" src="https://github.com/automote/thingSocket/blob/master/flow-chart.png" />
</p>

<p>
While in AP mode SSID and Password shoud be given using the following API
	<ul>
	<li>http://192.168.4.1/a?ssid="yourSSID"&pass="yourPSKkey"</li>
	</ul>
A webpage is also provided for entering SSID and Password if you are using the browser method.	
</p>
The ESP in thingSocket acts as Webserver on port 80 and has the following APIs
</p>
<p>
The server will set a GPIO pin depending upon the request
	<ol>
		<li>http://server_ip/plug/read will read all the status of all the GPIOs used</li>
		<li>http://server_ip/plug/1/0 will set the GPIO14 low</li>
		<li>http://server_ip/plug/1/1 will set the GPIO14 high</li>
		<li>http://server_ip/plug/2/0 will set the GPIO12 low</li>
		<li>http://server_ip/plug/2/1 will set the GPIO12 high</li>
		<li>http://server_ip/plug/3/0 will set the GPIO13 low</li>
		<li>http://server_ip/plug/3/1 will set the GPIO13 high</li>
		<li>http://server_ip/plug/4/0 will set the GPIO15 low</li>
		<li>http://server_ip/plug/4/1 will set the GPIO15 high</li>
	</ol>
Other functionality include
	<ul>
		<li>http://server_ip/factoryreset will clear the EEPROM contents. Its serves the purpose of factory resetting the device</li>
		<li>http://server_ip/reboot will reboot the device after 10 seconds</li>
		<li>http://server_ip/setappliance will set the appliance location, type and name</li>
	</ul>
where <I>server_ip</I> is the IP address of the ESP8266 module, will be printed to Serial when the module is connected.
* v0.5 is the standalone version
* v0.6 is currently developed version
* other sketches are the modules which were used in the v0.5 and v0.6
* v0.7 is in developement which makes use of Wi-Fi Manager by <i>tzapu</i> https://github.com/tzapu/WiFiManager

</p>

## Hardware
The hardware is kept simple and has been designed in Eagle for anyone to use the schematic and board files to develop his own product. We are even planing to provide GERBER files once we ready with the Design. 
<p>
	<ul>
		<li>For Geeks, they can directly buy the hardware from us or can make their own device</li>
		<li>For Tinkerers, UART pins are provided to flash the code of their choice or improve upon the already existing code</li>
		<li>For ESP Extremists, they can flash NodeMCU firmware and start coding in Lua. Example codes will be provided soon</li>
	</ul>
</p>

### Bugs
Code contains bugs some of which are critical while others are non critical
<p>
	<ul>
		<li>Critical:</li>
		<ol>
			<li>while submitting the SSID and password the URL doesnot encode spacial characters</li>
		</ol>
		<li>Non-Critical:</li>
		<ol>
			<li>Previous SSID and password still remains in memory even after factory reset</li>
			<li><strike>multicast packets are not broken into separate packets</strike></li>
			<li><strike>string of appl_type is not getting parsed properly</strike></li>
		</ol>
	</ul>
</p>

#### Inspiration
- http://www.esp8266.com/viewtopic.php?f=29&t=2520