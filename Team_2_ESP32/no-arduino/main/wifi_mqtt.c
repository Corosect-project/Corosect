#include "wifi_mqtt.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "lwip/dns.h"
#include "mqtt_client.h"

#define ESP_WIFI_SSID   CONFIG_WIFI_SSID
#define ESP_WIFI_PASS   CONFIG_WIFI_PASSWORD 
#define ESP_MAX_RETRY   CONFIG_WIFI_RETRY_MAX

#define MQTT_TIMEOUT    CONFIG_MQTT_TIMEOUT
#define MQTT_MAX_RETRY  CONFIG_MQTT_MAX_RETRY


#if CONFIG_WIFI_ALL_CHANNEL_SCAN
#define WIFI_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
#elif CONFIG_WIFI_FAST_SCAN
#define WIFI_SCAN_METHOD WIFI_FAST_SCAN
#else
#define WIFI_SCAN_METHOD WIFI_FAST_SCAN
#endif /* CONFIG_WIFI_SCAN_METHOD */

#define MQTT_BROKER     CONFIG_MQTT_BROKER

#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN


static EventGroupHandle_t s_wifi_event_group;
static EventGroupHandle_t s_mqtt_event_group;

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

#define MQTT_CONNECTED_BIT  BIT0
#define MQTT_FAIL_BIT       BIT1

static const char *TAG = "wifi";
static const char *MQTT_TAG = "MQTT";
static int s_retry_num = 0;
static int s_mqtt_retry = 0;

static esp_mqtt_client_handle_t client;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if(error_code != 0){
        ESP_LOGE(MQTT_TAG,"Last error %s: 0x%x", message,error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(MQTT_TAG,"Event dispatched from event loop bse=%s, event_id=%ld",base,event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch((esp_mqtt_event_id_t)event_id){
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(MQTT_TAG,"MQTT_EVENT_CONNECTED");
            xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
            s_mqtt_retry = 0;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(MQTT_TAG,"MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(MQTT_TAG,"MQTT_EVENT_SUBSCRIBED, msg_id=%d",event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(MQTT_TAG,"MQTT_EVENT_UNSUBSCRIBED, msg_id=%d",event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(MQTT_TAG,"MQTT_EVENT_PUBLISHED, msg_id=%d",event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(MQTT_TAG,"MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\rn", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(MQTT_TAG,"MQTT_EVENT_ERROR");
            if(event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT){
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(MQTT_TAG,"Last errno string (%s)",strerror(event->error_handle->esp_transport_sock_errno));

                /* esp-tls 0x8006 ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT */
                if(event->error_handle->esp_tls_last_esp_err == 0x8006){
                    if(s_mqtt_retry < MQTT_MAX_RETRY){
                        ESP_LOGI(MQTT_TAG,"TLS timeout error, trying again");
                        esp_mqtt_client_start(client);
                        ++s_mqtt_retry;
                    }else{
                        ESP_LOGI(MQTT_TAG,"TLS timeout error, tried max");
                        xEventGroupSetBits(s_mqtt_event_group, MQTT_FAIL_BIT);
                    }
                }else{
                        xEventGroupSetBits(s_mqtt_event_group, MQTT_FAIL_BIT);
                }
            break;
        default:
            ESP_LOGI(MQTT_TAG,"other event id:%d",event->event_id);
            break;
    }
}
}


static void event_handler(void *arg, esp_event_base_t event_base, 
        int32_t event_id, void *event_data)
{
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START){
        esp_wifi_connect();
    }else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
        if(s_retry_num < ESP_MAX_RETRY){
            esp_wifi_connect();
            ++s_retry_num;
            ESP_LOGI(TAG,"retry to connect to the AP");
        }else{
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    }else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
        ip_event_got_ip_t *event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG,"got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


esp_err_t mqtt_app_start()
{

    s_mqtt_event_group = xEventGroupCreate();

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER,
        .network.reconnect_timeout_ms = MQTT_TIMEOUT,
        .network.timeout_ms = MQTT_TIMEOUT,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    EventBits_t bits = xEventGroupWaitBits(s_mqtt_event_group, MQTT_CONNECTED_BIT | MQTT_FAIL_BIT, pdFALSE,pdFALSE, portMAX_DELAY);

    if(bits & MQTT_CONNECTED_BIT){
        ESP_LOGI(MQTT_TAG,"Succesfully connected MQTT");
        return 0;
    }else if(bits & MQTT_FAIL_BIT){
        ESP_LOGI(MQTT_TAG,"MQTT connection failed");
        return -1;
    }else{
        ESP_LOGE(MQTT_TAG,"Error in MQTT connection");
        return -1;
    }


}

void mqtt_send_result(uint16_t val, char *topic){
    /* Since we're working with 16 bit values
     * the maximum character count can't exceed 5 
     * so a 5 character buffer is enough to hold our results */
    char sResult[6];
    snprintf(sResult, 6, "%d", val);

    /* setting len to 0 here calculates length from payload 
     * this may be slightly less efficient than using a constant of 6
     * but using a constant seems to sometimes result in garbled output
     * so it's probably better to leave it like this */
    esp_mqtt_client_publish(client, topic, sResult, 0, 0, 0); /* client, topic, data, len, qos, retain*/
}


esp_err_t wifi_init_sta()
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();


    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                &event_handler, NULL,&instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                &event_handler, NULL,&instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
            .scan_method = WIFI_SCAN_METHOD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG,"wifi init sta finished");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);


    if(bits & WIFI_CONNECTED_BIT){
        ESP_LOGI(TAG,"connected to %s",ESP_WIFI_SSID);
        return 0;
    }else if(bits & WIFI_FAIL_BIT){
        ESP_LOGI(TAG,"failed to connect to %s",ESP_WIFI_SSID);
        return -1;
    }else{
        ESP_LOGE(TAG,"this shouldn't happen");
        return -1;
    }
}
