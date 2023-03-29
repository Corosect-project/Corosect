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

#define LED_GPIO CONFIG_LED_GPIO
#define SENSOR_POWER_GPIO CONFIG_SENSOR_POWER_GPIO

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

void go_to_sleep(int ms){
    gpio_set_level(SENSOR_POWER_GPIO,0); /* Disable power output for CO2 sensor */

    /* esp_sleep_pd_config results in an assertion error so explicitly disable all wakeup sources
     * this _should_ ensure that ESP_PD_OPTION_AUTO powers down all unnecessary domains 
     * FIXME: missing something obvious with esp_sleep_pd_config so this is a hacky solution*/
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    /* enable wakeup sources here */
    esp_sleep_enable_timer_wakeup(ms*1000);
    esp_deep_sleep_start();
}

static inline esp_err_t disable_crc(){
    const uint8_t cmd[2] = {0x37, 0x68};
    esp_err_t ret = i2c_write_cmd(I2C_CO2_ADDR, cmd, sizeof(cmd));
    return ret;
}

static inline esp_err_t self_test(uint8_t *data){
    const uint8_t cmd[2] = {0x36,0x5B};
    esp_err_t ret = i2c_write_cmd(I2C_CO2_ADDR, cmd, sizeof(cmd));
    if(ret != ESP_OK) return ret;

    /* Self-test guaranteed to take <20ms */
    vTaskDelay(20 / portTICK_PERIOD_MS);
    ret = i2c_read(I2C_CO2_ADDR, data, 2);
    return ret;
}

static inline esp_err_t set_binary_gas(){
    /* 0x3615 set binary gas to 0x0001 co2 in air 0-100%  */
    const uint8_t cmd[4] = {0x36, 0x15, 
                            /* args*/
                            0x00, 0x01};
    esp_err_t ret = i2c_write_cmd(I2C_CO2_ADDR, cmd, sizeof(cmd));
    return ret;

}

/* Measure gas and temperature */
static esp_err_t measure_gas(uint8_t *data){
    /* 0x3639 measure gas concentration */
    const uint8_t cmd[2] = {0x36,0x39};
    esp_err_t ret = i2c_write_cmd(I2C_CO2_ADDR, cmd ,sizeof(cmd));

    if(ret != ESP_OK) return ret;

    /* Measurement should take at most 66ms */
    vTaskDelay(70 / portTICK_PERIOD_MS);

    ret = i2c_read(I2C_CO2_ADDR, data, sizeof(data));

    return ret;
}

static void inline configure_pins(){
    /* TODO: working LED everything */
    // const uint32_t led_pin = 1 << LED_GPIO;


    /* Create bit mask for CO2 sensor power pin */
    const uint32_t power_pin = 1 << SENSOR_POWER_GPIO;

    /* For now we just need to enable output on 1 pin
     * this can be expanded to include more */
    const uint32_t pins_mask = power_pin;

    gpio_config_t conf ={
        .pin_bit_mask = pins_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&conf);

}

void initialize_i2c(){
    uint8_t data[2]={-1};

    int test = i2c_master_init();
    puts("i2c init");
    printf("driver install %s\n",esp_err_to_name(test));

    /* We won't be error checking at this point
     * so disable CRC to reduce transmission overhead */
    test = disable_crc();
    printf("disable crc %s\n",esp_err_to_name(test));

    /* Setting binary gas to CO2 in air 0-100% 
     * TODO: add enum and config for choosing between different available options */
    test = set_binary_gas();
    printf("set binary gas %s\n\n",esp_err_to_name(test));

    test = self_test(data);
    if((data[0] << 8 | data[1]) != 0){ 
        ESP_LOGE(TAG,"Sensor selftest failure"); 
        go_to_sleep(ERR_SLEEP_MS);
    }
    printf("selftest result: %d %d\n",data[0], data[1]);
}



void app_main(void){
    uint8_t data[4]={0};
    uint16_t co2_result;
    uint16_t temp_result;
    int test;

    configure_pins();
    /* enable power output here so sensor has time to run through boot 
     * this can be moved later in the sequence if the ~3mA saved during WiFi/MQTT setup is worth it and doesn't require wait later on */
    gpio_set_level(SENSOR_POWER_GPIO,1); /* Power output for CO2 sensor */

    nvs_flash_init();
    test = wifi_init_sta();
    /* wifi_init_sta() will return 0 on success
     * otherwise connecting failed */
    if(test != 0){
        ESP_LOGI(TAG,"Wifi connection failed, sleeping");
        go_to_sleep(ERR_SLEEP_MS);
    }

    ESP_LOGI(TAG,"Got wifi connection, starting MQTT");

    test = mqtt_app_start();
    /* mqtt_app_start will also return 0 on success
     * otherwise connecting failed */
    if(test != 0){
        ESP_LOGI(TAG,"MQTT connection failed, sleeping");
        go_to_sleep(ERR_SLEEP_MS);
    }

    ESP_LOGI(TAG,"Got mqtt connection, starting I2C");

    /* Succesfully connected to WiFi and MQTT, time to start measuring */
    initialize_i2c();

    while(1){
        for(int i = 0; i < SAMPLES; i++){
            measure_gas(data);
            co2_result = data[0] << 8 | data[1];
            temp_result = data[2] << 8 | data[3];
            mqtt_send_result(co2_result, "co2");
            mqtt_send_result(temp_result, "temp");
            printf("co2: %d \t temp: %d\n",co2_result, temp_result);
            /* Measure should only be called once every second */
            vTaskDelay(MEASURE_INTERVAL / portTICK_PERIOD_MS);
        }

        ESP_LOGI(TAG,"Measured, going to sleep");
        go_to_sleep(SLEEP_MS);

    }
}
