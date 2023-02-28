#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "sdkconfig.h"


/* Set SDA and SCL pins to 6 and 7
 * Default pins incompatible with changing LED color */
#define I2C_MASTER_SDA_IO 6
#define I2C_MASTER_SCL_IO 7

/* RTC maximum possible SCL frequency is 1MHz */
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
static esp_err_t disableCRC(){
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
static esp_err_t selftest(uint8_t *data_h, uint8_t *data_l){
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
static esp_err_t setBinaryGas(){
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

/* Send command 0x3639 to STC31 sensor 
 * measure gas concentration and read result
 * gas concentration to *co2_data
 * temperature to *temp_data */
static esp_err_t measureGas(uint16_t *co2_data, uint16_t *temp_data){
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

void initializeI2C(){
    uint8_t data_h=1, data_l=1;

    int test = i2c_master_init();
    printf("driver install %s\n",esp_err_to_name(test));

    test = disableCRC();
    printf("disablecrc %s\n",esp_err_to_name(test));

    test = selftest(&data_h, &data_l);
    printf("selftest %s\n",esp_err_to_name(test));
    printf("selftest result: %d%d\n",data_h, data_l);

    test = setBinaryGas();
    printf("set binary gas %s\n",esp_err_to_name(test));
}


void app_main(void){
    uint16_t co2,temp;

    initializeI2C();

    for(;;){
        measureGas(&co2,&temp);
        printf("%hu \t %hu\n",co2,temp);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }



}

