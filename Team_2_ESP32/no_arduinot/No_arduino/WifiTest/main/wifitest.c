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

#define ESP_WIFI_SSID   "panoulu"
#define ESP_WIFI_PASS    ""
#define ESP_MAX_RETRY   10

#define MQTT_BROKER      "mqtt://100.64.254.11"

#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN


static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

static const char *TAG = "wifi";
static const char *MQTT_TAG = "MQTT";
static int s_retry_num = 0;


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
            msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "dataa tulee", 0, 1, 0);
            ESP_LOGI(MQTT_TAG, "sent publis succesful, msg_id=%d", msg_id);
            while(1){
                vTaskDelay(1000 /portTICK_PERIOD_MS);
            msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "dataa tulee", 0, 1, 0);
            ESP_LOGI(MQTT_TAG, "sent publis succesful, msg_id=%d", msg_id);

            }

            /*            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
                          ESP_LOGI(MQTT_TAG, "sent subscribe succesful, msg_id=%d", msg_id);

                          msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
                          ESP_LOGI(MQTT_TAG,"sent unsubscribe succesful, msg_id=%d",msg_id);*/
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(MQTT_TAG,"MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(MQTT_TAG,"MQTT_EVENT_SUBSCRIBED, msg_id=%d",event->msg_id);
            msg_id = esp_mqtt_client_publish(client,"/topic/qos0","data", 0, 0, 0);
            ESP_LOGI(MQTT_TAG,"Sent publish succesful, msg_id=%d", msg_id);
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
            }
            break;

        default:
            ESP_LOGI(MQTT_TAG,"other event id:%d",event->event_id);
            break;
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


static void mqtt_app_start()
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}



void wifi_init_sta()
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
                &event_handler, NULL,
                &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                &event_handler, NULL,
                &instance_got_ip));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
            //            .threshold_authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
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

    }else if(bits & WIFI_FAIL_BIT){
        ESP_LOGI(TAG,"failed to connect to %s",ESP_WIFI_SSID);
    }else{
        ESP_LOGE(TAG,"this shouldn't happen");
    }
}


void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG,"ESP_WIFI_MODE_STA");
    wifi_init_sta();
    mqtt_app_start();

}

