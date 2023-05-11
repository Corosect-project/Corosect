# Build instructions
Get ESP-IDF from [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html#ide) following the instructions for your preferred operating system and tool(s).

## Build instructions for GNU/Linux.

First, make sure [environment variables are set](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/linux-macos-setup.html#get-started-set-up-env)

Then set target board to `esp32c3` with 
```
idf.py set-target esp32c3
```

### Run to configure
```
idf.py menuconfig
```
Project related configuration can be found under `Corosect project configuration --->`

![](menuconfig.png)

#### You MUST configure at least the following:
```
Corosect project configuration ---> WiFi configuration ---> WiFi SSID           # WiFi SSID is required for the program to function, password may be left blank in case of an open network
Corosect project configuration ---> MQTT configuration ---> MQTT Broker URI     # MQTT broker URI is also required, note this needs to be a URI in the form of mqtt://broker.address:port
```
Optional configuration for I2C can be found under `Corosect project configuration ---> I2C configuration`, by default the program will use GPIO 3 for sensor power, GPIO 6 for SDA and GPIO 7 for SCL with a master clock frequency of 1MHz and assumes 0x29 as the sensor address.
Indicator LED can be enabled by setting [x] Enable indicator LED under `Corosect project configuration --->`. Will by default use the ESP32C3 builtin LED on GPIO 8 and has only been tested with the ESP32C3 builtin LED but might work with external ones.

Sleep timings and intervals between measurements when awake can also be configured under `Corosect project configuration --->`.

### Build with
```
idf.py build            # build
idf.py -p [PORT] flash    # build and flash board.
```

#### Open serial monitor
```
idf.py -p [PORT] monitor          # open serial monitor
idf.py -p [PORT] flash monitor    # build, flash board and open serial monitor
```
`Ctrl + ]` to exit the serial console.

