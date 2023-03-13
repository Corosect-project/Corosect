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
 * Default pins incompatible with internal LED pin */
#define I2C_MASTER_SDA_IO 6
#define I2C_MASTER_SCL_IO 7

/* RTC maximum SCL frequency is 1MHz */
#define I2C_MASTER_FREQ_HZ 1000000

/* Address for Click Co2 sensor */
#define I2C_CO2_ADDR 0x29 


static const char* TAG = "espi";

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

/* Write 16 bit command to i2c slave */
static esp_err_t i2c_write(uint8_t addr, uint8_t cmdh, uint8_t cmdl){
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

/* Write 16 bit command to i2c slave with 16 bit argument */
static esp_err_t i2c_write_with_arg(uint8_t addr, uint8_t cmdh, uint8_t cmdl, uint8_t argh, uint8_t argl){
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

/* 
 * Improved i2c write function, work in progress
 *
static esp_err_t i2c_write_cmd(uint8_t addr, const uint8_t *data, size_t len){
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr << 1 | 0x0, 0x1);
    i2c_master_write(cmd, data, len, 0x1);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}*/

static esp_err_t i2c_read(uint8_t addr, uint8_t *data, size_t len){
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr << 1 | 0x1, 0x1);
    i2c_master_read(cmd, data, len, 0x0);
    i2c_master_write_byte(cmd, addr<<1 | 0x1, 0x0); /* Seems necessary, otherwise everything past first byte read will be 255 */
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ret;

}

static inline esp_err_t disable_crc(){
    esp_err_t ret = i2c_write(I2C_CO2_ADDR, 0x37, 0x68);
    return ret;
}


static inline esp_err_t self_test(uint8_t *data){
    esp_err_t ret = i2c_write(I2C_CO2_ADDR, 0x36, 0x5B); /* 0x365B run self test */
    if(ret != ESP_OK) return ret;

    /* Self-test guaranteed to take <20ms */
    vTaskDelay(20 / portTICK_PERIOD_MS);
    ret = i2c_read(I2C_CO2_ADDR, data, 2);
    return ret;
}

/* Sets binary gas to co2 in air 0 to 100 vol%*/
static inline esp_err_t set_binary_gas(){
    esp_err_t ret;
    ret = i2c_write_with_arg(I2C_CO2_ADDR, 0x36, 0x15, 0x00, 0x01);
    return ret;

}

/* Measure gas and temperature */
static esp_err_t measure_gas(uint8_t *data){
    esp_err_t ret = i2c_write(I2C_CO2_ADDR, 0x36 ,0x39);
    if(ret != ESP_OK) return ret;

    /* Measurement should take at most 66ms */
    vTaskDelay(70 / portTICK_PERIOD_MS);

    ret = i2c_read(I2C_CO2_ADDR, data, 4);

    return ret;
}


/* Send sleep command 0x3677 to sensor*/
static inline esp_err_t sensor_sleep(){
    esp_err_t ret = i2c_write(I2C_CO2_ADDR, 0x36, 0x77);
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
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&conf);

}


void initialize_i2c(){
    uint8_t data[2]={0};

    int test = i2c_master_init();
    puts("i2c init");
    printf("driver install %s\n",esp_err_to_name(test));

    test = disable_crc();
    printf("disable crc %s\n",esp_err_to_name(test));

    test = set_binary_gas();
    printf("set binary gas %s\n\n",esp_err_to_name(test));

    test = self_test(data);
    printf("selftest %s\n",esp_err_to_name(test));
    printf("selftest result: %d %d\n",data[0], data[1]);
}


void go_to_sleep(int ms){
    esp_sleep_enable_timer_wakeup(ms*1000);
    esp_deep_sleep_start();

}

void app_main(void){
    uint8_t data[6]={0};
    int test;

    initialize_i2c();
    test = measure_gas(data);
    printf("Measure: %s\n",esp_err_to_name(test));
    printf("co2 Data: %d\n",(data[0]<<8|data[1]));
    printf("temp data: %d\n\n",(data[2]<<8|data[3]));

    while(1){
        measure_gas(data);
        printf("co2: %d \t temp: %d\n",(data[0]<<8|data[1]), (data[2]<<8|data[3]));
//        printf("co2:%d %d %d\t temp: %d %d %d\n",data[0],data[1],data[2],data[3],data[4],data[5]);

        vTaskDelay(1000 / portTICK_PERIOD_MS);

    }

    //    configure_led();

    /*    while(1){
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

          }*/

}

