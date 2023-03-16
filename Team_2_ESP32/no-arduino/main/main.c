/* 
 * Copyright (c) 2023 OAMK Corosect-project.
 * 
 * I2C implementation on ESP-ID. Heavy work in progress.*/

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_sleep.h"
#include "nvs_flash.h"

#include "esp32_i2c.h"
#include "wifi_mqtt.h"

#define LED_GPIO 8

/* Set SDA and SCL pins to 6 and 7
 * Default pins incompatible with internal LED pin
#define I2C_MASTER_SDA_IO 6
#define I2C_MASTER_SCL_IO 7

 RTC maximum SCL frequency is 1MHz 
#define I2C_MASTER_FREQ_HZ 1000000

 Address for Click Co2 sensor */
#define I2C_CO2_ADDR CONFIG_I2C_CO2_ADDR


static const char* TAG = "espi";


/* MQTT client used to send messages */
esp_mqtt_client_handle_t mqtt_client;

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

    ret = i2c_read(I2C_CO2_ADDR, data, sizeof(data));

    return ret;
}


/* Send sleep command 0x3677 to sensor*/
static inline esp_err_t sensor_sleep(){
    esp_err_t ret = i2c_write(I2C_CO2_ADDR, 0x36, 0x77);
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

/*void sendResult(uint16_t val, char *topic){
    TODO: Replace this with a static buffer at some point
    int len = snprintf(NULL,0,"%d",val); 
    char* sResult = (char*)malloc((len+1)*sizeof(char));
    snprintf(sResult, len+1, "%d", val);

    esp_mqtt_client_publish(mqtt_client, topic, sResult, len, 0, 0);
}*/

void app_main(void){
    uint8_t data[4]={0};
    uint16_t co2_result;
    uint16_t temp_result;
    int test;

    initialize_i2c();
    test = measure_gas(data);
    printf("Measure: %s\n",esp_err_to_name(test));
    printf("co2 Data: %d\n",(data[0]<<8|data[1]));
    printf("temp data: %d\n\n",(data[2]<<8|data[3]));
    nvs_flash_init();
    wifi_init_sta();
    mqtt_app_start();

    while(1){
        measure_gas(data);
        co2_result = data[0] << 8 | data[1];
        temp_result = data[2] << 8 | data[3];
        mqtt_send_result(co2_result, "co2");
        mqtt_send_result(temp_result, "temp");
        printf("co2: %d \t temp: %d\n",(data[0]<<8|data[1]), (data[2]<<8|data[3]));
        vTaskDelay(1000 / portTICK_PERIOD_MS);

    }

}

