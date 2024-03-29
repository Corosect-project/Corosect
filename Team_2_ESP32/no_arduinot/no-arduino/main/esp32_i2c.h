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

/* Write len byte command to I2C slave
 * addr: slave address
 * data: command and arguments to be written
 *
 * data should be an array of commands and related arguments to write
 * supports writing 8 bit and 16 bit long commands
 * with 8 bit and 16 bit long arguments
 *
 * returns 0 on success
 */
esp_err_t i2c_write_cmd(uint8_t addr, const uint8_t *data, size_t len);


/* Read len bytes from I2C slave
 * addr: slave address
 * *data: array to store received bytes in
 * len: number of bytes to read from slave
 *
 * returns 0 on success
 */
esp_err_t i2c_read(uint8_t addr, uint8_t *data, size_t len);


#endif // _ESP32_I2C_H
