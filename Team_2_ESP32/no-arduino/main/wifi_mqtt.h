#ifndef _WIFI_MQTT_H
#define _WIFI_MQTT_H

#include "mqtt_client.h"

/* Starts WiFi in station mode */
void wifi_init_sta(void);

/* starts MQTT connection */
void mqtt_app_start(void);

/* Publishes 2 byte uint to specified topic
 * val: 16 bit value to send
 * topic: topic for publishing */
void mqtt_send_result(uint16_t val, char *topic);

#endif //_WIFI_MQTT_H
