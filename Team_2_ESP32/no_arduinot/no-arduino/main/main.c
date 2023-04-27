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
#define I2C_MASTER_FREQ_HZ 1000000*/

/* Address for Click Co2 sensor */
#define I2C_CO2_ADDR CONFIG_I2C_CO2_ADDR

/* Number of measurements every cycle */
#define SAMPLES CONFIG_SAMPLES

/* Time between each measurement (ms). Should be >= 1000 */
#define MEASURE_INTERVAL CONFIG_MEASURE_INTERVAL

/* Sleep time between succesful measurements (ms) */
#define SLEEP_MS CONFIG_SLEEP_MS

/* Sleep time in case of a failure (ms) */
#define ERR_SLEEP_MS CONFIG_ERR_SLEEP_MS



static const char* TAG = "espi";


/* MQTT client used to send messages  
esp_mqtt_client_handle_t mqtt_client;*/

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

static inline esp_err_t sensor_wake(){
    esp_err_t ret = i2c_wakeup_sensor(I2C_CO2_ADDR);
    /* Sensor needs time to wake up, 50ms seems reliable */
    vTaskDelay(50 / portTICK_PERIOD_MS); 
    return ret;

}

/*static void configure_led(){
    gpio_config_t conf ={
        .pin_bit_mask = 0x100,  Bit mask for GPIO 8 
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&conf);

}*/

void initialize_i2c(){
    uint8_t data[2]={-1};

    int test = i2c_master_init();
    puts("i2c init");
    printf("driver install %s\n",esp_err_to_name(test));

    test = sensor_wake();
    printf("sensor wake %s\n",esp_err_to_name(test));

    test = disable_crc();
    printf("disable crc %s\n",esp_err_to_name(test));

    test = set_binary_gas();
    printf("set binary gas %s\n\n",esp_err_to_name(test));

    test = self_test(data);
    if((data[0] << 8 | data[1]) != 0){ ESP_LOGE(TAG,"Sensor selftest failure");};
    printf("selftest sent %s\n",esp_err_to_name(test));
    printf("selftest result: %d %d\n",data[0], data[1]);
}


void go_to_sleep(int ms){
    sensor_sleep();
    esp_sleep_enable_timer_wakeup(ms*1000);
    esp_deep_sleep_start();
}

void app_main(void){
    uint8_t data[4]={0};
    uint16_t co2_result;
    uint16_t temp_result;
    int test;

    nvs_flash_init();
    test = wifi_init_sta();
    /* wifi_init_sta() will return 0 on success
     * otherwise connecting failed */
    if(test != 0){
        ESP_LOGI(TAG,"Wifi connection failed, sleeping");
        go_to_sleep(ERR_SLEEP_MS);
    }

    ESP_LOGI(TAG,"Got wifi connection, starting MQTT");
    mqtt_app_start();
    initialize_i2c();
    test = measure_gas(data);
    printf("Measure: %s\n",esp_err_to_name(test));
    printf("co2 Data: %d\n",(data[0]<<8|data[1]));
    printf("temp data: %d\n\n",(data[2]<<8|data[3]));


    while(1){
        for(int i = 0; i < SAMPLES; i++){
            measure_gas(data);
            co2_result = data[0] << 8 | data[1];
            temp_result = data[2] << 8 | data[3];
            mqtt_send_result(co2_result, "co2");
            mqtt_send_result(temp_result, "temp");
            printf("co2: %d \t temp: %d\n",(data[0]<<8|data[1]), (data[2]<<8|data[3]));
            /* Measure should only be called once every second */
            vTaskDelay(MEASURE_INTERVAL / portTICK_PERIOD_MS);
        }

        ESP_LOGI(TAG,"Measured, going to sleep");
        go_to_sleep(SLEEP_MS);


    }
}
