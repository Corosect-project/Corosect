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

/* Set SDA and SCL pins to configured ones, by default 6 & 7
 * Default pins 8 & 9 incompatible with internal LED pin */
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

esp_err_t i2c_write_cmd(uint8_t addr, const uint8_t *data, size_t len){
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

