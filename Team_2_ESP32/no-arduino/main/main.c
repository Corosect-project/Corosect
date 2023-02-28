/* 
 * Copyright (c) 2023 OAMK Corosect-project.
 * 
 * I2C implementation on ESP-ID. Heavy work in progress.*/

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_sleep.h"

#define LED_GPIO 8

/* Set SDA and SCL pins to 6 and 7
 * Default pins incompatible with changing LED color */
#define I2C_MASTER_SDA_IO 6
#define I2C_MASTER_SCL_IO 7

/* RTC maximum SCL frequency is 1MHz */
#define I2C_MASTER_FREQ_HZ 1000000

/* Address for Click Co2 sensor */
#define I2C_CO2_ADDR 0x29 


static esp_err_t i2c_master_init(){
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

/* Send command 0x3768 to STC31 sensor
 * disables CRC for data transmissions as it is not required for this project */
static esp_err_t disable_crc(){
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, I2C_CO2_ADDR<<1 | 0x0, 0x1);
    i2c_master_write_byte(cmd, 0x37, 0x1);
    i2c_master_write_byte(cmd, 0x68, 0x1);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(0, cmd, 1000/portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

/* Send command 0x365B to STC31 sensor
 * runs automatic self-test on sensor and saves result to *data_h and *data_l */
static esp_err_t self_test(uint8_t *data_h, uint8_t *data_l){
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, I2C_CO2_ADDR<<1 | 0x0, 0x1);
    i2c_master_write_byte(cmd, 0x36, 0x1); 
    i2c_master_write_byte(cmd, 0x5B, 0x1);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(0, cmd, 1000/portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if(ret != ESP_OK) return ret;

    /* Self-test guaranteed to take <20ms */
    vTaskDelay(20 / portTICK_PERIOD_MS);

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, I2C_CO2_ADDR << 1 | 0x1, 0x1);
    i2c_master_read_byte(cmd, data_h, 0x0);
    i2c_master_read_byte(cmd, data_l, 0x1);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ret;

}

/* Send command 0x3615 with arg 0x0001
 * set binary gas to CO2 in air 0 to vol% */
static esp_err_t set_binary_gas(){
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, I2C_CO2_ADDR<<1 | 0x0, 0x1);

    /* Command */
    i2c_master_write_byte(cmd, 0x36, 0x1); 
    i2c_master_write_byte(cmd, 0x15, 0x1); 
    /* Arg */
    i2c_master_write_byte(cmd, 0x00, 0x1); 
    i2c_master_write_byte(cmd, 0x01, 0x1); 

    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ret;

}

/* Send command 0x3639 
 * measure gas concentration and read result
 * gas concentration to *co2_data
 * temperature to *temp_data */
static esp_err_t measure_gas(uint16_t *co2_data, uint16_t *temp_data){
    uint8_t co2_h,co2_l,temp_h,temp_l;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, I2C_CO2_ADDR<<1 | 0x0, 0x1);
    i2c_master_write_byte(cmd, 0x36, 0x1); 
    i2c_master_write_byte(cmd, 0x39, 0x1);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(0, cmd, 1000/portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if(ret != ESP_OK) return ret;

    /* Measurement should take at most 66ms */
    vTaskDelay(70 / portTICK_PERIOD_MS);
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, I2C_CO2_ADDR<<1 | 0x1, 0x1);
    i2c_master_read_byte(cmd, &co2_h, 0x0); /* Read high byte for co2 measurement */
    i2c_master_read_byte(cmd, &co2_l, 0x0); /* Read low byte for co2 measurement */

    i2c_master_read_byte(cmd, &temp_h, 0x0); /* Read high byte for temperature measurement */
    i2c_master_read_byte(cmd, &temp_l, 0x1); /* Read low byte for temperature measurement */
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    *co2_data = (uint16_t)co2_h << 8 | (uint16_t)co2_l;
    *temp_data = (uint16_t)temp_h << 8 | (uint16_t)temp_l;

    return ret;


}

/*TODO: clean this up, combine sleep and wakeup functions potentially */

/* Send sleep command 0x3677 to sensor
 * otherwise wake device up from sleep by sending addr + write bit */
static esp_err_t sensor_sleep(){
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, I2C_CO2_ADDR<<1 | 0x0, 0x1);
        i2c_master_write_byte(cmd, 0x36, 0x1);
        i2c_master_write_byte(cmd, 0x77, 0x1);

    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    /* 12ms wakeup for sensor */
    vTaskDelay(12 / portTICK_PERIOD_MS);
    return ret;


}

/* Run wakeup sequence*/
static esp_err_t sensor_wake(){
    int test; 
    puts("\nSensor wakeup");
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, I2C_CO2_ADDR<<1 | 0x0, 0);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    /* 50ms wakeup for sensor seems safe */
    vTaskDelay(50 / portTICK_PERIOD_MS);


    /* Sensor enters default mode upon wakeup, disable crc and set binary gas */
    test = disable_crc();
    printf("disable crc %s\n",esp_err_to_name(test));

    test = set_binary_gas();
    printf("set binary gas %s\n\n",esp_err_to_name(test));

    return ret;

}

static void configure_led(){
    gpio_config_t conf ={
        .pin_bit_mask = 0x100, /* Bit mask for GPIO 8 */
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&conf);

}


void initialize_i2c(){
    uint8_t data_h=1, data_l=1;

    int test = i2c_master_init();
    puts("i2c init");
    printf("driver install %s\n",esp_err_to_name(test));

    test = self_test(&data_h, &data_l);
    printf("selftest %s\n",esp_err_to_name(test));
    printf("selftest result: %d%d\n",data_h, data_l);

    sensor_wake();
}


void go_to_sleep(int ms){
    esp_sleep_enable_timer_wakeup(ms*1000);
    esp_deep_sleep_start();

}


void app_main(void){
    uint16_t co2,temp;

    initialize_i2c();
    configure_led();

    while(1){
        sensor_wake();
        gpio_set_level(LED_GPIO,true);

        for(int i = 0; i < 10; i++){
            measure_gas(&co2,&temp);
            printf("%hu \t %hu\n",co2,temp);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }

        vTaskDelay(500 / portTICK_PERIOD_MS);

        puts("Going to sleep");
        gpio_set_level(LED_GPIO,false);
        sensor_sleep();
        go_to_sleep(5000);

        vTaskDelay(5000 / portTICK_PERIOD_MS);

    }

}

