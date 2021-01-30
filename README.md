# Simple and lightweight C++ ESP32 BLE wrapper

For reverse engineering my CBT-Power LiFePo Battery BLE enabled BMS (selled as Creabest in Europe) I used the raw GATT-Server example provided by Espressif inside the ESP-IDF for Visual Studio Code.

They have a tutorial (which is quite ok but doesn't describe how to use if for multiple services) and an API documentation, but at least for the BLE component the documentation is nearly useless (December 2020) - it looks like a repeatition of the function and structure names as sentences. I guess the developers were short of time or it was written by someone else who didn't really know much about it.

However, not the best for starting completely new with BLE (like I did at this time). Fortunately I'm used to develop in C/C++ over two decades now, so the provided example code itself wasn't that hard to understand.

The BLE service idea is made for OOP and the raw C interface provided by espressif is a lot of writing the same stuff over and over again, so I decided to write a partially object oriented interface with low memory usage (not really more than the C example uses) and event handler support.

# Server

Here's a simple usage example creating two services with attributes and handling events on it.

[TODO]


