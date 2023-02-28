# Build instructions
Get ESP-IDF from [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html#ide) following the instructions for your preferred operating system and tool(s).

## Following instructions are for GNU/Linux.

Set target board to esp32c3 with `idf.py set-target esp32c3`

1. Run `idf.py menuconfig` to configure if required (should currently run with default configuration).

2. Build with `idf.py build` or run `idf.py -p PORT flash` to build and flash board.

3. Open serial monitor with `idf.py -p PORT monitor` or flash and open monitor `idf.py -p PORT flash monitor`.

`Ctrl + ]` to exit the serial console.


