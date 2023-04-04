#include "led_control.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"

/* Pin used for LED strip
 * 8 is the internal LED pin on the devkit */
#define LED_GPIO CONFIG_LED_GPIO

static led_strip_handle_t led_strip;
static const char *LED_TAG = "ledi";

void configure_led(void){
    ESP_LOGI(LED_TAG,"Configuring led pin");
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1, /* using only a single LED */
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 100, /* 10MHz because it's used in the example */
    };
    ESP_LOGI(LED_TAG,"Configured rmt");

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
    ESP_LOGI(LED_TAG,"Configured everything");

}

void set_led_color(int r, int g, int b){
    ESP_LOGI(LED_TAG,"Setting color to %d %d %d",r,g,b);
    led_strip_set_pixel(led_strip, 0, r, g, b);
    led_strip_refresh(led_strip);
}

void clear_led(void){
    ESP_LOGI(LED_TAG,"Cleared LED");
    led_strip_clear(led_strip);
}


