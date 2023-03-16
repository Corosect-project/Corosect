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
#include <zephyr/net/net_config.h>
#include <zephyr/net/socket.h>
LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

#include "config.h"
#include "mqtt.h"
#define DISABLE_NH3
#ifndef DISABLE_NH3
#include "nh3_sensor.h"
#endif

#define ERROR(err) (err < 0)
#define ZEPHYR_ADDR "2001:db8::1"
#define SERVER_ADDR "2001:db8::2"
#define SERVER_PORT 1883

volatile bool quit = false;
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(0x1820)),
};

void button_handler(uint32_t state, uint32_t has_changed);
void bt_ready();
static void bt_connected(struct bt_conn *conn, uint8_t err);
static void bt_disconnected(struct bt_conn *conn, uint8_t reason);

BT_CONN_CB_DEFINE(con_calbacks) = {
    .connected = bt_connected,
    .disconnected = bt_disconnected};

void init_sockaddresses(struct sockaddr *my_addr, struct sockaddr *peer) {
  // Set zephyr address
  memset(my_addr, 0, sizeof(*my_addr));
  struct sockaddr_in6 *addr6 = net_sin6(my_addr);
  addr6->sin6_family = AF_INET6;
  addr6->sin6_port = htons(1885);
  int32_t err = net_addr_pton(AF_INET6, ZEPHYR_ADDR, &addr6->sin6_addr);
  if (ERROR(err)) LOG_ERR("ADDR, %d", err);

  // Set peer address
  memset(peer, 0, sizeof(*peer));
  struct sockaddr_in6 *peer6 = net_sin6(peer);
  peer6->sin6_family = AF_INET6;
  peer6->sin6_port = htons(1885);
  err = net_addr_pton(AF_INET6, SERVER_ADDR, &peer6->sin6_addr);
  if (ERROR(err)) LOG_ERR("PEER, %d", err);
}

void main(void) {
  int32_t err;
  LOG_INF("Hello World! %s", CONFIG_BOARD);

  if (ERROR(dk_buttons_init(button_handler)))
    LOG_ERR("Error initializing buttons");

  err = bt_enable(NULL);
  if (ERROR(err)) LOG_ERR("Error enabling BT %d", err);
  bt_ready();

  LOG_DBG("Waiting connection...");
  k_sleep(K_MSEC(30000));
  LOG_DBG("Continuing");
  init_network(ZEPHYR_ADDR);

  struct sockaddr addr;
  struct sockaddr peer;
  init_sockaddresses(&addr, &peer);

  int sock = zsock_socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
  if (ERROR(sock)) LOG_ERR("%d, err: %d", sock, errno);

  uint8_t buffer[256] = {0};
  struct zsock_pollfd tcpfds[1];
  tcpfds[0].events = ZSOCK_POLLIN;
  tcpfds[0].fd = sock;
  tcpfds[0].revents = 0;

  zsock_connect(sock, &peer, sizeof(peer));
  zsock_send(sock, "Kusi mutteri", sizeof("Kusi mutteri"), 0);

  while (true) {
    err = zsock_poll(tcpfds, 1, 10000);
    if (ERROR(err)) {
      LOG_ERR("Poll error, %d", errno);
      break;
    } else if (err > 0) {
      if (tcpfds[0].revents & ZSOCK_POLLIN) {
        ssize_t received = zsock_recv(sock, buffer, 256, 0);
        if (received == 0) {
          LOG_DBG("Connection closed");
          break;
        } else if (received > 0) {
          LOG_HEXDUMP_WRN(buffer, received, "DATA:");
          break;
        } else
          LOG_ERR("Error on receiving, %d", errno);
      }
    } else {
      LOG_DBG("Poll timedout");
      LOG_DBG("revent, %d", tcpfds[0].revents);
    }
  }
  err = zsock_close(sock);
  if (ERROR(err)) LOG_ERR("Socket clsoe error, %d", errno);

  sock = zsock_socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
  LOG_ERR("%d, err: %d", sock, errno);

  err = zsock_bind(sock, &addr, sizeof(addr));
  if (ERROR(err)) LOG_ERR("Bind, %d", errno);
  zsock_listen(sock, 1);
  if (ERROR(err)) {
    LOG_ERR("Listen, %d", errno);
    return;
  }
  while (true) {
    LOG_INF("LOOP");
    struct sockaddr client_addr;
    socklen_t addrlen;
    int client = zsock_accept(sock, &client_addr, &addrlen);
    LOG_ERR("%d", errno);
    do {
      ssize_t received = zsock_recv(client, buffer, sizeof(buffer), MSG_WAITALL);
      LOG_ERR("%d", errno);
      if (ERROR(received))
        LOG_ERR("Recev error, %d", received);
      else if (received == 0) {
        LOG_INF("Sock closed");
        break;
      } else {
        LOG_HEXDUMP_INF(buffer, received, "BUFFER DATA:");
        zsock_send(client, buffer, received, 0);
      }
    } while (true);
  }

  struct mqtt_client *client_ctx = init_mqtt(SERVER_ADDR, SERVER_PORT);

  for (size_t i = 0; i < 11; i++) {
    err = mqtt_connect(client_ctx);
    if (err != 0) {
      LOG_ERR("MQTT ERROR: %d", err);
      while (!quit) {
        k_sleep(K_MSEC(1000));
      }
      return;
    }

    zsock_poll(fds, 1, 10000);
    err = mqtt_input(client_ctx);
    if (ERROR(err)) LOG_ERR("Input error, %d", err);

    if (!connected) {
      err = mqtt_abort(client_ctx);
      LOG_ERR("Connection aborted, %d", err);
    } else
      break;
  }

  if (connected) {
    uint8_t *message = "Hello world from Zephyr";
    uint8_t *topic = "TEST";
    err = send_message(message, topic);
    if (err) LOG_ERR("SEND ERROR: %d", err);
  }

#ifndef DISABLE_NH3
  init_nh3();
#endif

  while (!quit && connected) {
#ifndef DISABLE_NH3
    read_and_print_nh3();
#endif
    zsock_poll(fds, 1, 5000);
    err = mqtt_input(client_ctx);
    LOG_ERR("%d", err);
    err = mqtt_live(client_ctx);
    LOG_ERR("%d", err);
    LOG_DBG("WAITING");
    k_sleep(K_MSEC(2000));
  }

  LOG_INF("QUIT\n");
  mqtt_disconnect(client_ctx);
  bt_le_adv_stop();
  bt_disable();
}

void button_handler(uint32_t state, uint32_t has_changed) {
  if (DK_BTN2_MSK == has_changed) {
    quit = true;
  }
}

void bt_ready() {
  uint32_t err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), 0, 0);
  if (err == -ENOMEM)
    LOG_ERR("ENOMEM\n");
  else if (err == -ECONNREFUSED || err == -EIO)
    LOG_ERR("ECONNREFUSED\n");
  LOG_DBG("%d\n", err);
}

void bt_connected(struct bt_conn *conn, uint8_t err) {
  if (err) {
    LOG_ERR("Connection failed, %d", err);
  } else {
    LOG_INF("Connected");
    // init_network();
  }
}

void bt_disconnected(struct bt_conn *conn, uint8_t reason) {
  LOG_INF("Disconnected, reason %d", reason);
}
