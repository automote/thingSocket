# smart-plug
This is smart plug Wi-Fi enabled socket open for people to develop or duplicate.

Smart Plug is am attempt to promote IoT by providing open and hackable devices that you use everyday. Devices like bulbs and sockets. The idea revolves around identifying everyday use devices and convert them into IoT devices by adding Wi-Fi, BLE or ZigBee capabilities.

We are starting with Wi-Fi Socket and Wi-Fi Bulb. These devices have the crowd favourite ESP modules and we are planning to keep the hardware and software designs open. This would encourage school children, college students and even IoT enthusiasts to get into the IoT Bandwagon.

# Software
The software is completely written in Arduino IDE, so anyone familiar with Arduino can tinker with the code and personalise the device according to his requirement.
The ESP in smart-plug acts as Webserver on port 80 and has the following APIs
The server will set a GPIO pin depending on the request
1)  http://server_ip/plug/read will read all the status of all the GPIOs used,
2)  http://server_ip/plug/1/0 will set the GPIO14 low,
3)  http://server_ip/plug/1/1 will set the GPIO14 high
4)  http://server_ip/plug/2/0 will set the GPIO12 low,
5)  http://server_ip/plug/2/1 will set the GPIO12 high
6)  http://server_ip/plug/3/0 will set the GPIO13 low,
7)  http://server_ip/plug/3/1 will set the GPIO13 high
8)  http://server_ip/plug/4/0 will set the GPIO15 low,
9)  http://server_ip/plug/4/1 will set the GPIO15 high
where <I>server_ip</I> is the IP address of the ESP8266 module, will be printed to Serial when the module is connected.

# Hardware
The hardware is kept simple and has been designed in Eagle for anyone to use the schematics and board file to make his own product. We are even planing to provide GERBER files once we finalise the Design. Geeks can directly buy the hardware from us or can make their own.
For Tinkerers, UART pins are provided to flash the code of their choice or improve upon the already existing code.
For ESP Extremists, they can flash NodeMCU firmware and start coding in Lua. Example codes will be provided soon.
