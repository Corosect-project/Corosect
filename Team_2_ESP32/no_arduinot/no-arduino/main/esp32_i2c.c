/* 
 * Copyright (c) 2023 OAMK Corosect-project.
 * 
 * I2C implementation on ESP-IDF. Heavy work in progress.*/

#include "esp32_i2c.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_sleep.h"

/* Following macros can be set via `idf.py menuconfig` (recommended)
 * or directly here in the source file */

/* Set SDA and SCL pins to configured ones, by default 6 & 7
 * Default ESP32C3 pins 8 & 9 incompatible with internal LED pin 8 */
#define I2C_MASTER_SDA_IO CONFIG_I2C_MASTER_SDA
#define I2C_MASTER_SCL_IO CONFIG_I2C_MASTER_SCL

/* RTC maximum SCL frequency is 1MHz */
#define I2C_MASTER_FREQ_HZ CONFIG_I2C_MASTER_FREQ

/* Tag for debugging */
/*static const char* TAG = "esp_i2c";*/

esp_err_t i2c_master_init(void){
    int i2c_master_port = I2C_NUM_0;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        .clk_flags = 0,
    };

    esp_err_t err = i2c_param_config(i2c_master_port, &conf);
    if(err != ESP_OK){
        return err;
    }

    /* I2C master doesn't need RX or TX buffers */
    return i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);

}

<<<<<<< HEAD:Team_2_ESP32/no_arduinot/no-arduino/main/esp32_i2c.c
/* Write 16 bit command to i2c slave 
 * TODO: ugly solution. Make into a generic write function that can handle n length commands and args*/
esp_err_t i2c_write(uint8_t addr, uint8_t cmdh, uint8_t cmdl){
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create(); 
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr<<1 | 0x0, 0x1);
    i2c_master_write_byte(cmd, cmdh, 0x1);
    i2c_master_write_byte(cmd, cmdl, 0x1);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(0, cmd, 1000/portTICK_PERIOD_MS);
//    ESP_LOGI(TAG, "Wrote command, response %s",esp_err_to_name(ret));
    i2c_cmd_link_delete(cmd);

    return ret;
}

/* Write 16 bit command to i2c slave with 16 bit argument
 * TODO: ugly solution. Make into a generic write function that can handle n length commands and args*/
esp_err_t i2c_write_with_arg(uint8_t addr, uint8_t cmdh, uint8_t cmdl, uint8_t argh, uint8_t argl){
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create(); 
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr<<1 | 0x0, 0x1);
    i2c_master_write_byte(cmd, cmdh, 0x1);
    i2c_master_write_byte(cmd, cmdl, 0x1);
    i2c_master_write_byte(cmd, argh, 0x1);
    i2c_master_write_byte(cmd, argl, 0x1);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(0, cmd, 1000/portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ret;

}

/* Sends wakeup command (address + write bit) to given slave */
esp_err_t i2c_wakeup_sensor(uint8_t addr){
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr << 1 | 0x0, 0x0);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(0,cmd,1000/portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    return ret;

}

/* 
 * Improved i2c write function, work in progress
 *
static esp_err_t i2c_write_cmd(uint8_t addr, const uint8_t *data, size_t len){
=======
esp_err_t i2c_write_cmd(uint8_t addr, const uint8_t *data, size_t len){
>>>>>>> 06884eb70aa5ab6e031c6902cf977979e104b4df:Team_2_ESP32/no-arduino/main/esp32_i2c.c
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr << 1 | 0x0, 0x1);
    i2c_master_write(cmd, data, len, 0x1);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

esp_err_t i2c_read(uint8_t addr, uint8_t *data, size_t len){
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr << 1 | 0x1, 0x1);
    i2c_master_read(cmd, data, len, 0x2); /* 0x2 write NACK after last byte */
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

