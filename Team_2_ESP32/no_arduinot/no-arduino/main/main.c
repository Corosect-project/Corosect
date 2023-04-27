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
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"

#include "esp32_i2c.h"
#include "wifi_mqtt.h"

/* The following macros can be set either via `idf.py menuconfig` (recommended)
 * or directly here in the source file */

/* Pin configuration */
#define SENSOR_POWER_GPIO CONFIG_SENSOR_POWER_GPIO

<<<<<<< HEAD:Team_2_ESP32/no_arduinot/no-arduino/main/main.c
 RTC maximum SCL frequency is 1MHz 
#define I2C_MASTER_FREQ_HZ 1000000*/
=======
/* define LED GPIO only if usage is enabled in config */
#ifdef CONFIG_ENABLE_LED
#define LED_GPIO CONFIG_LED_GPIO
#endif //CONFIG_ENABLE_LED
>>>>>>> 06884eb70aa5ab6e031c6902cf977979e104b4df:Team_2_ESP32/no-arduino/main/main.c

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
<<<<<<< HEAD:Team_2_ESP32/no_arduinot/no-arduino/main/main.c


=======
>>>>>>> 06884eb70aa5ab6e031c6902cf977979e104b4df:Team_2_ESP32/no-arduino/main/main.c

static const char* TAG = "espi";

#ifdef CONFIG_ENABLE_LED
static led_strip_handle_t led_strip;
#endif

<<<<<<< HEAD:Team_2_ESP32/no_arduinot/no-arduino/main/main.c
/* MQTT client used to send messages  
esp_mqtt_client_handle_t mqtt_client;*/
=======
/* shouldn't be called at all if LED is not enabled so
 * get rid of entire function */
#ifdef CONFIG_ENABLE_LED
static inline void configure_led(void){
    ESP_LOGI(TAG, "Configuring LED pin");
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
}
#endif //CONFIG_ENABLE_LED

static inline void configure_pins(){
    /* Create bit mask for CO2 sensor power pin */
    const uint32_t power_pin = 1 << SENSOR_POWER_GPIO;

    /* For now we're only using a single pin but a mask can be added here
     * and expanded if it is required */

    gpio_config_t conf ={
        .pin_bit_mask = power_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&conf);

    /* only configure LED if it's enabled */
#ifdef CONFIG_ENABLE_LED
    /* Configure the LED pin */
    configure_led();
#endif //CONFIG_ENABLE_LED
}

/* Set LED color to 
 * r: red
 * g: green
 * b: blue
 * */
static inline void set_led_color(int r, int g, int b){
    /* set to nop if LED usage is disabled 
     * won't cause issues if code calls this 
     * but doesn't have LED usage enabled*/
#ifdef CONFIG_ENABLE_LED
    led_strip_set_pixel(led_strip, 0, r, g, b);
    led_strip_refresh(led_strip);
#endif //CONFIG_ENABLE_LED
}

static inline void clear_led(void){
#ifdef CONFIG_ENABLE_LED
    led_strip_clear(led_strip);
#endif//CONFIG_ENABLE_LED
}

static inline void go_to_sleep(int ms){
    gpio_set_level(SENSOR_POWER_GPIO,0); /* Disable power output for CO2 sensor */
    clear_led();                         /* turn off LED in sleep */

    /* esp_sleep_pd_config results in an assertion error so explicitly disable all wakeup sources
     * this _should_ ensure that ESP_PD_OPTION_AUTO powers down all unnecessary domains 
     * FIXME: missing something obvious with esp_sleep_pd_config so this is a hacky solution*/
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    /* enable wakeup sources here */
    esp_sleep_enable_timer_wakeup(ms*1000);

    esp_deep_sleep_start();
}
>>>>>>> 06884eb70aa5ab6e031c6902cf977979e104b4df:Team_2_ESP32/no-arduino/main/main.c

static inline esp_err_t disable_crc(){
    /* 0x3768 disable crc */
    const uint8_t cmd[2] = {0x37, 0x68};
    esp_err_t ret = i2c_write_cmd(I2C_CO2_ADDR, cmd, sizeof(cmd));
    return ret;
}

static inline esp_err_t self_test(uint8_t *data){
    /* 0x365B run sensor selftest */
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
static inline esp_err_t measure_gas(uint8_t *data){
    /* 0x3639 measure gas concentration */
    const uint8_t cmd[2] = {0x36,0x39};
    esp_err_t ret = i2c_write_cmd(I2C_CO2_ADDR, cmd ,sizeof(cmd));

    if(ret != ESP_OK) return ret;

    /* Measurement should take at most 66ms */
    vTaskDelay(70 / portTICK_PERIOD_MS);

    ret = i2c_read(I2C_CO2_ADDR, data, sizeof(data));

    return ret;
}


<<<<<<< HEAD:Team_2_ESP32/no_arduinot/no-arduino/main/main.c
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
=======
static void initialize_i2c(){
    /* A good result will be 0 so initialize 
     * to a known bad value here */
    uint8_t data[2]={255,255};
>>>>>>> 06884eb70aa5ab6e031c6902cf977979e104b4df:Team_2_ESP32/no-arduino/main/main.c

    int test = i2c_master_init();
    puts("i2c init");
    printf("driver install %s\n",esp_err_to_name(test));

<<<<<<< HEAD:Team_2_ESP32/no_arduinot/no-arduino/main/main.c
    test = sensor_wake();
    printf("sensor wake %s\n",esp_err_to_name(test));

=======
    /* We won't be needing error checking in this project
     * so disable CRC to reduce transmission overhead */
>>>>>>> 06884eb70aa5ab6e031c6902cf977979e104b4df:Team_2_ESP32/no-arduino/main/main.c
    test = disable_crc();
    printf("disable crc %s\n",esp_err_to_name(test));

    /* Setting binary gas to CO2 in air 0-100% 
     * TODO: add enum and config for choosing between different available options */
    test = set_binary_gas();
    printf("set binary gas %s\n\n",esp_err_to_name(test));

    test = self_test(data);
<<<<<<< HEAD:Team_2_ESP32/no_arduinot/no-arduino/main/main.c
    if((data[0] << 8 | data[1]) != 0){ ESP_LOGE(TAG,"Sensor selftest failure");};
    printf("selftest sent %s\n",esp_err_to_name(test));
    printf("selftest result: %d %d\n",data[0], data[1]);
}


void go_to_sleep(int ms){
    sensor_sleep();
    esp_sleep_enable_timer_wakeup(ms*1000);
    esp_deep_sleep_start();
}
=======
    if(((data[0] << 8) | data[1]) != 0){ 
        ESP_LOGE(TAG,"Sensor selftest failure"); 
        set_led_color(255, 0, 0);
        go_to_sleep(ERR_SLEEP_MS);
    }
    printf("selftest result: %d%d\n",data[0], data[1]);
}


>>>>>>> 06884eb70aa5ab6e031c6902cf977979e104b4df:Team_2_ESP32/no-arduino/main/main.c

void app_main(void){
    uint8_t data[4]={0};
    uint16_t co2_result;
    uint16_t temp_result;
    int test;

<<<<<<< HEAD:Team_2_ESP32/no_arduinot/no-arduino/main/main.c
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

=======
    configure_pins();
    /* enable power output here so sensor has time to run through boot 
     * this can be moved later in the sequence if the ~3mA saved during WiFi/MQTT setup is worth it and doesn't require wait later on */
    gpio_set_level(SENSOR_POWER_GPIO,1); /* Power output for CO2 sensor */
>>>>>>> 06884eb70aa5ab6e031c6902cf977979e104b4df:Team_2_ESP32/no-arduino/main/main.c

    /* Initialize NVS */
    test = nvs_flash_init();
    if(test == ESP_ERR_NVS_NO_FREE_PAGES || test == ESP_ERR_NVS_NEW_VERSION_FOUND){
        nvs_flash_erase();
        test = nvs_flash_init();
    }
<<<<<<< HEAD:Team_2_ESP32/no_arduinot/no-arduino/main/main.c
=======

    ESP_ERROR_CHECK(test);

    test = wifi_init_sta();
    /* wifi_init_sta() will return 0 on success
     * otherwise connecting failed */
    if(test != 0){
        ESP_LOGE(TAG,"Wifi connection failed, sleeping");
        set_led_color(16, 0, 0);
        go_to_sleep(ERR_SLEEP_MS);
    }

    ESP_LOGI(TAG,"Got wifi connection, starting MQTT");
    set_led_color(0, 16, 0);

    test = mqtt_app_start();
    /* mqtt_app_start will also return 0 on success
     * otherwise connecting failed */
    if(test != 0){
        ESP_LOGE(TAG,"MQTT connection failed, sleeping");
        go_to_sleep(ERR_SLEEP_MS);
    }

    ESP_LOGI(TAG,"Got mqtt connection, starting I2C");

    /* Succesfully connected to WiFi and MQTT, time to start measuring.
     * Will automatically sleep if an error occurs during sensor selftest */
    initialize_i2c();

    set_led_color(5,0,16);

    while(1){
        for(int i = 0; i < SAMPLES; i++){
            measure_gas(data);
            co2_result = data[0] << 8 | data[1];
            temp_result = data[2] << 8 | data[3];
            mqtt_send_result(co2_result, "co2");
            mqtt_send_result(temp_result, "temp");
            printf("co2: %u\t temp: %u\n",co2_result, temp_result);
            /* Measure should only be called once every second */
            vTaskDelay(MEASURE_INTERVAL / portTICK_PERIOD_MS);
        }

        ESP_LOGI(TAG,"Measured, going to sleep");
        go_to_sleep(SLEEP_MS);


        /* this should never ever be hit
         * but break out of loop just in case
         * so program doesn't end up stuck measuring */
        break;

    }
    /* broke out of loop, try to sleep once again
     * in case previous sleep failed for some reason */
    go_to_sleep(SLEEP_MS);
>>>>>>> 06884eb70aa5ab6e031c6902cf977979e104b4df:Team_2_ESP32/no-arduino/main/main.c
}
