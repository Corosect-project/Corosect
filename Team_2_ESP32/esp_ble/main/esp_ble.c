#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"


char *TAG = "ESP_BLE";

/*******************
 * BLE GAP RELATED *
 *******************/
#define DEVICE_NAME "espi"

/* test purpose manufacturer data */
static uint8_t manufacturer[3] = {'M','I','E'};

/* advertising parameters for GAP */
static esp_ble_adv_params_t adv_params = {
    .adv_int_min    = 0x100,
    .adv_int_max    = 0x100,
    .adv_type       = ADV_TYPE_IND,
    .own_addr_type  = BLE_ADDR_TYPE_RPA_PUBLIC,
    .channel_map    = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};


/* advertising data config */
static esp_ble_adv_data_t adv_config = {
    .set_scan_rsp = false, /* this is not scan response data */
    .include_txpower = true,
    .include_name = true, /* include name */
    .min_interval = 0x0006, /* slave conn min interval. t = min_interval * 1.25ms */
    .max_interval = 0x0010, /* slave conn max interval. t = max_interval * 1.25ms */
    .appearance = 0x00,
    /* Manufacturer data might get cut off by the 31 byte limit */
    .manufacturer_len = sizeof(manufacturer), /* length of manufacturer data */
    .p_manufacturer_data = manufacturer, /* manufacturer data */
    .service_data_len = 0,
    .p_service_data = NULL,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

/* scan response config */
static esp_ble_adv_data_t rsp_config = {
    .set_scan_rsp = true,
    .include_name = true,
    .manufacturer_len = sizeof(manufacturer),
    .p_manufacturer_data = manufacturer,

};


/*********************
 * BLE GATTS RELATED *
 *********************/

/* UUIDs for test */
#define GATTS_SERVICE_UUID 0x00ff
#define GATTS_CHAR_UUID_TEST 0xff01

/* declare gatts event handler */
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/* data used for testing */
static uint8_t test_str[] = {0x11,0x22,0x33};

static esp_gatt_char_prop_t property = 0;

static esp_attr_value_t gatts_test_val ={
    .attr_max_len = 0x40,
    .attr_len = sizeof(test_str),
    .attr_value = test_str,
};

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

static struct gatts_profile_inst test_app_profile = {
    .gatts_cb = gatts_event_handler,
    .gatts_if = ESP_GATT_IF_NONE,

};

static void ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param){
    switch(event){
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: /* advertise data set */
            ESP_LOGI(TAG, "BLE advertising data set");
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: /* R A W advertise data set */
            ESP_LOGI(TAG,"RAW BLE advertising data set");
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: /* advertising started */
            ESP_LOGI(TAG,"Advertising started");
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: /* advertising stopped */
            ESP_LOGI(TAG,"Advertising stopped");
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT: /* scan response data set */
            ESP_LOGI(TAG,"Scan response data set");
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT: /* scan response raw data set */
            ESP_LOGI(TAG,"raw Scan response data set");
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(TAG,"updated connection params, status %d, min_int = %d, max_int = %d, conn_int %d, latency %d, timeout %d\n",
                    param->update_conn_params.status,
                    param->update_conn_params.min_int,
                    param->update_conn_params.max_int,
                    param->update_conn_params.conn_int,
                    param->update_conn_params.latency,
                    param->update_conn_params.timeout);
            break;

            /* These are probably irrelevant since we only need to advertise
             * but keep them here for now just in case they're required */
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: /* scan parameters set */
        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: /* scan started */
        case ESP_GAP_BLE_SCAN_RESULT_EVT: /* scan result comes in */
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: /* scan stopped */
            break;

        default:
            break;

    }
}

void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param){
    switch(event){
        esp_err_t ret;
        case ESP_GATTS_REG_EVT: /* application ID registered */
            ESP_LOGI(TAG,"Register app event, status %d, id %d\n", param->reg.status, param->reg.app_id);
            if(param->reg.status == ESP_GATT_OK){
                test_app_profile.gatts_if = gatts_if;
            }else{
                ESP_LOGI(TAG, "Register app failed, id %04x, status %d\n",param->reg.app_id, param->reg.status);
                return;
            }
            test_app_profile.service_id.is_primary = true;
            test_app_profile.service_id.id.inst_id = 0x00;
            test_app_profile.service_id.id.uuid.len = ESP_UUID_LEN_16;
            test_app_profile.service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID;


            ret = esp_ble_gap_set_device_name(DEVICE_NAME);
            if(ret){ESP_LOGE(TAG,"Set devname error %x",ret);}

            ret = esp_ble_gap_config_adv_data(&adv_config);
            if(ret){ESP_LOGE(TAG,"Config adv data error %x",ret);}

            esp_ble_gatts_create_service(gatts_if, &test_app_profile.service_id, 4);
            break;
        case ESP_GATTS_CREATE_EVT: /* service creation complete */
            ESP_LOGI(TAG, "Create service evt status %d handle %d\n", param->create.status, param->create.service_handle);
            test_app_profile.service_handle = param->create.service_handle;
            test_app_profile.char_uuid.len = ESP_UUID_LEN_16;
            test_app_profile.char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_TEST;

            esp_ble_gatts_start_service(test_app_profile.service_handle);
            property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
            ret = esp_ble_gatts_add_char(test_app_profile.service_handle,
                                        &test_app_profile.char_uuid,
                                        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                        property,
                                        &gatts_test_val,
                                        NULL);

            break;
        case ESP_GATTS_DISCONNECT_EVT: /* GATT client disconnects */
            ESP_LOGI(TAG,"Gatts disconnected, starting advertising again\n");
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GATTS_ADD_CHAR_EVT:{ /* adding a characteristic complete */
            uint16_t length = 0;
            const uint8_t *prf_char;

            ESP_LOGI(TAG,"Add char evt status %d attr handle %d service_handle %d\n",
                    param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);

            test_app_profile.char_handle = param->add_char.attr_handle;
            test_app_profile.descr_uuid.len = ESP_UUID_LEN_16;
            test_app_profile.descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
            ret = esp_ble_gatts_get_attr_value(param->add_char.attr_handle, &length, &prf_char);
            if(ret == ESP_FAIL){ESP_LOGE(TAG,"ILLEGAL HANDLE");}

            ESP_LOGI(TAG,"char len =%x\n", length);

            for(int i = 0; i < length; i++){
                ESP_LOGI(TAG,"prf_char[%x] = %x\n",i,prf_char[i]);
            }
            ret = esp_ble_gatts_add_char_descr(test_app_profile.service_handle,
                                               &test_app_profile.descr_uuid,
                                               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                               NULL,NULL);
            if(ret){ ESP_LOGE(TAG,"add char descr failed code %x", ret);}
            break;
        }
        case ESP_GATTS_ADD_CHAR_DESCR_EVT: /* adding a descriptor complete */
            ESP_LOGI(TAG,"Add descr evt status %d attr_handle %d service_handle %d\n",
                    param->add_char.status, param->add_char.attr_handle,
                    param->add_char.service_handle);
            break;
        case ESP_GATTS_CONNECT_EVT:{ /* GATT client connects */
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            conn_params.latency = 0;
            conn_params.max_int = 0x30; /* 0x30*1.25ms = 40ms */
            conn_params.min_int = 0x10; /* 0x10*1.25ms = 20ms */
            conn_params.timeout = 400;  /* 400*10ms = 4000ms */

            ESP_LOGI(TAG,"esp gatts connect evt, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:\n",
                    param->connect.conn_id,
                   param->connect.remote_bda[0], 
                   param->connect.remote_bda[1], 
                   param->connect.remote_bda[2], 
                   param->connect.remote_bda[3], 
                   param->connect.remote_bda[4], 
                   param->connect.remote_bda[5]);
            test_app_profile.conn_id = param->connect.conn_id;

            esp_ble_gap_update_conn_params(&conn_params);
            break;
        }
        case ESP_GATTS_READ_EVT: /* application requests read */
            ESP_LOGI(TAG,"Read event conn_id %d, trans_id %lu, handle %d\n",
                    param->read.conn_id, param->read.trans_id, param->read.handle);
            esp_gatt_rsp_t rsp;
            memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
            rsp.attr_value.handle = param->read.handle;
            rsp.attr_value.len = 4;
            rsp.attr_value.value[0] = 0xde;
            rsp.attr_value.value[1] = 0xed;
            rsp.attr_value.value[2] = 0xbe;
            rsp.attr_value.value[3] = 0xef;
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
            break;
        case ESP_GATTS_WRITE_EVT: /* application requests write */
        case ESP_GATTS_EXEC_WRITE_EVT: /* gatt client requests execute write */
        case ESP_GATTS_MTU_EVT: /* MTU set complete */
        case ESP_GATTS_CONF_EVT: /* confirm received */
        case ESP_GATTS_UNREG_EVT: /* application ID unregistered */
        case ESP_GATTS_ADD_INCL_SRVC_EVT: /* included service added */
        case ESP_GATTS_DELETE_EVT:  /* service deleted */
        case ESP_GATTS_STOP_EVT: /* service stopped */
        case ESP_GATTS_OPEN_EVT: /* connected to peer */
        case ESP_GATTS_CANCEL_OPEN_EVT: /* disconnected from peer */
        case ESP_GATTS_CLOSE_EVT: /* gatt server closed */
        case ESP_GATTS_LISTEN_EVT: /* gatt listen connect */
        case ESP_GATTS_CONGEST_EVT: /* congestion */
        case ESP_GATTS_RESPONSE_EVT: /* gatt response complete */
        case ESP_GATTS_CREAT_ATTR_TAB_EVT: /* gatt create table complete */
        case ESP_GATTS_SET_ATTR_VAL_EVT: /* gatt set attr value complete */
        case ESP_GATTS_SEND_SERVICE_CHANGE_EVT: /* gatt send service change complete */
        default:
            break;

    }

}


void app_main(void){
    esp_err_t ret;

    /* initialize non-volatile storage */
    ret = nvs_flash_init();

    if(ret== ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
        nvs_flash_erase();
        ret = nvs_flash_init();
    }

    /* release memory before init */
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

    /* use macro to get default BT controller config and init BT controller */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);

    /* only using BLE here */
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);


    ret = esp_bluedroid_init();
    ret = esp_bluedroid_enable();

    /* GAP stuff */
    /* Register event handler as the callback function for GAP events */
    ret = esp_ble_gap_register_callback(ble_gap_event_handler);


    /* GATT server stuff */
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    ret = esp_ble_gatts_app_register(0);

}
