# Experiments with RISC-V assembly on the ESP32C3
## NOTE: both require the I2C controller to be configured before use
`i2c_testwrite.S` works currently and can be called from C or C++ source code with `i2c_write(addr)` to write pre-set test data to slave at address `addr` and returns the given slave address.

`i2c_write.S` is currently broken and is derefencing something it shouldn't be, not yet sure what.

## How to use
To add either of these (or any) assembly code to your project using ESP-IDF, simply place your `.S` files in your project's `main/` directory and add them to your `main/CMakelists.txt`.

For example: 
```
idf_component_register(SRCS "main.c" "i2c_testwrite.S" "led_blink.S"
                      INCLUDE_DIRS ".")
```



