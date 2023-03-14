#ifndef _ESP32_I2C_H
#define _ESP32_I2C_H

#include <stddef.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "driver/i2c.h"

/* Initialize i2c master on SDA, SCL on pins defined in config
 * Master frequency defined in config
 * returns 0 on succes */
esp_err_t i2c_master_init(void);

/* Write 16 bit command to I2C slave
 * addr: slave address
 * cmdh: MSB of command
 * cmdl: LSB of command 
 *
 * returns 0 on success
 */
esp_err_t i2c_write(uint8_t addr, uint8_t cmdh, uint8_t cmdl);

/* Write 16 bit command with 16 bit arg to I2C slave
 * addr: slave address
 * cmdh: MSB of command
 * cmdl: LSB of command 
 * argh: MSB of arg
 * argl: LSB of arg
 *
 * returns 0 on success
 */
esp_err_t i2c_write_with_arg(uint8_t addr, uint8_t cmdh, uint8_t cmdl, uint8_t argh, uint8_t argl);

/* Read len bytes from I2C slave
 * addr: slave address
 * *data: array to store received bytes in
 * len: number of bytes to read from slave
 *
 * returns 0 on success
 */
esp_err_t i2c_read(uint8_t addr, uint8_t *data, size_t len);

#endif // _ESP32_I2C_H
