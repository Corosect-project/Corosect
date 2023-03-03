/*
 * Copyright (c) 2023 OAMK Corosect-project.
 *
 * SPDX-License-Identifier: MIT
 */

#include <dk_buttons_and_leds.h>
#include <nrf52.h>
#include <nrfx_spim.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
LOG_MODULE_REGISTER(main_log, LOG_LEVEL_DBG);

#include "config.h"
#define DISABLE_NH3
#ifndef DISABLE_NH3
#include "nh3_sensor.h"
#endif

#define ERROR(err) (err < 0)
#define ZEPHYR_ADDR "2001:db8::1"
#define SERVER_ADDR "2001:db8::2"
#define SERVER_PORT 1883

volatile bool quit = false;
bool connected = false;
// MQTT
static uint8_t rx_buffer[256];
static uint8_t tx_buffer[256];
static struct mqtt_client client_ctx;
static struct sockaddr_storage broker;

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(0x1820)),
};

void button_handler(uint32_t state, uint32_t has_changed);
void bt_ready();

#ifndef DISABLE_NH3

#endif

void mqtt_evt_handler(struct mqtt_client *client, const struct mqtt_evt *evt);

int sock;
struct sockaddr my_addr;
char buffer[256] = {0};
struct sockaddr sender;
socklen_t addr_len;

int32_t err;
void main(void) {
  LOG_INF("Hello World! %s", CONFIG_BOARD);

  init_network();

  // sock = zsock_socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
  // if (sock < 0) {
  //   err = errno;
  //   LOG_ERR("Couldn't craete ICMP socket, %d", err);
  //   return;
  // }

  // struct sockaddr_in6 *my_addr6 = (struct sockaddr_in6 *)&my_addr;
  // my_addr6->sin6_family = AF_INET6;
  // zsock_inet_pton(AF_INET6, "2001:db8::1", &my_addr6->sin6_addr);

  // err = zsock_bind(sock, &my_addr, sizeof(my_addr));
  // if (err < 0) {
  //   LOG_ERR("Failed to bind socket, %d", err);
  //   return;
  // }

  // struct zsock_pollfd fds2[1];
  // fds2[0].fd = sock;
  // fds2[0].events = ZSOCK_POLLIN;

  // while (true) {
  //   zsock_poll(fds2, 1, 5000);
  //   zsock_recvfrom(sock, buffer, 256, 0, &sender, &addr_len);
  //   LOG_HEXDUMP_DBG(buffer, 256, "recive buffer");
  // }

  if (ERROR(dk_buttons_init(button_handler)))
    LOG_ERR("Error initializing buttons");

  err = bt_enable(NULL);
  if (ERROR(err)) LOG_ERR("Error enabling BT %d", err);
  bt_ready();

  LOG_DBG("Waiting for connection...");
  // k_sleep(K_MSEC(60000));
  LOG_DBG("Stopped waitting");

  struct sockaddr_in6 *broker6 = (struct sockaddr_in6 *)&broker;
  broker6->sin6_family = AF_INET6;
  broker6->sin6_port = htons(SERVER_PORT);
  err = net_addr_pton(AF_INET6, SERVER_ADDR, &broker6->sin6_addr);
  LOG_DBG("TEST: %d", err);
  // zsock_inet_pton(AF_INET6, SERVER_ADDR, &broker6->sin6_addr);

  uint8_t *client_id = "ZEPHYR_mqtt_client";
  mqtt_client_init(&client_ctx);
  client_ctx.broker = &broker;
  client_ctx.evt_cb = mqtt_evt_handler;
  client_ctx.client_id.utf8 = client_id;
  client_ctx.client_id.size = sizeof(client_id) - 1;
  client_ctx.password = NULL;
  client_ctx.user_name = NULL;
  client_ctx.transport.type = MQTT_TRANSPORT_NON_SECURE;

  client_ctx.rx_buf = rx_buffer;
  client_ctx.rx_buf_size = sizeof(rx_buffer);
  client_ctx.tx_buf = tx_buffer;
  client_ctx.tx_buf_size = sizeof(tx_buffer);

  int rc = mqtt_connect(&client_ctx);
  if (rc != 0) {
    LOG_ERR("MQTT ERROR: %d", rc);
    while (!quit) {
      k_sleep(K_MSEC(1000));
    }
    return;
  }
  struct zsock_pollfd fds[1];
  fds[0].fd = client_ctx.transport.tcp.sock;
  fds[0].events = ZSOCK_POLLIN;
  zsock_poll(fds, 1, 5000);

  mqtt_input(&client_ctx);

  if (!connected) {
    mqtt_abort(&client_ctx);
  }

  if (connected) {
    uint8_t *message = "Hello world from Zephyr";
    uint8_t *topic = "TEST";
    struct mqtt_publish_param param = {0};
    param.message.payload.data = message;
    param.message.payload.len = sizeof(message) - 1;
    param.message.topic.qos = MQTT_QOS_0_AT_MOST_ONCE;
    param.message.topic.topic.utf8 = topic;
    param.message.topic.topic.size = sizeof(topic) - 1;
    mqtt_publish(&client_ctx, &param);
  }

#ifndef DISABLE_NH3
  init_nh3();
#endif

  while (!quit && connected) {
#ifndef DISABLE_NH3
    read_and_print_nh3();
#endif
    zsock_poll(fds, 1, 5000);
    mqtt_input(&client_ctx);
    mqtt_live(&client_ctx);
    k_sleep(K_MSEC(2000));
  }

  LOG_INF("QUIT\n");
  mqtt_disconnect(&client_ctx);
  bt_le_adv_stop();
  bt_disable();
}

void button_handler(uint32_t state, uint32_t has_changed) {
  if (DK_BTN2_MSK == has_changed) {
    quit = true;
  }
}

void bt_ready() {
  int err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), 0, 0);
  if (err == -ENOMEM)
    LOG_ERR("ENOMEM\n");
  else if (err == -ECONNREFUSED || err == -EIO)
    LOG_ERR("ECONNREFUSED\n");

  LOG_DBG("%d\n", err);
}

void mqtt_evt_handler(struct mqtt_client *client, const struct mqtt_evt *evt) {
  switch (evt->type) {
    case MQTT_EVT_CONNACK:
      if (evt->result != 0) {
        LOG_ERR("Error connecting: %d", evt->result);
        break;
      }
      LOG_INF("MQTT connected");
      connected = true;
      break;
    case MQTT_EVT_DISCONNECT:
      LOG_INF("MQTT disconnected");
      connected = false;
      break;
    case MQTT_EVT_PINGRESP:
      LOG_INF("Ping response");
      break;
    default:
      break;
  }
}
