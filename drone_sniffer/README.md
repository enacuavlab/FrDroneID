# Wifi drone beacon sniffer

Firmware for an ESP32.
Listen to beacon wifi frames to detect drones that broadcast their position according to the french regulation.


## How to use

Clone the [esp-idf](https://github.com/espressif/esp-idf) repository on your PC, and set the `IDF_PATH` environnement variable.

Then as usual :

`mkdir build && cd build`

`cmake ..`

`make`

Connect the ESP32 to your PC

`make flash`
