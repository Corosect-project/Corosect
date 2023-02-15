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

#define DISABLE_NH3
LOG_MODULE_REGISTER(main_log, LOG_LEVEL_DBG);

#define ERROR(err) (err < 0)
#define ZEPHYR_ADDR "192.168.1.5"
#define SERVER_ADDR "3.120.89.246"
#define SERVER_PORT 8000

volatile bool quit = false;
bool connected = false;
// MQTT
static uint8_t rx_buffer[256];
static uint8_t tx_buffer[256];
static struct mqtt_client client_ctx;
static struct sockaddr_storage broker;

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(0x0023)),
};

void button_handler(uint32_t state, uint32_t has_changed);
void bt_ready();

#ifndef DISABLE_NH3
#define CS_PIN 27
#define SCK_PIN 26
#define SDO_PIN 2

struct nh3_spi_data {
  bool OL;
  bool OH;
  uint32_t value;
};

void set_cs();
void clear_cs();
int read_nh3_data(nrfx_spim_t *instance, nrfx_spim_xfer_desc_t *transf);

void print_spi_data(uint8_t *spi_data, size_t size);
void format_nh3_data(uint8_t *spi_data, struct nh3_spi_data *_out_data);
void print_nh3_struct(struct nh3_spi_data *data);
#endif

void mqtt_evt_handler(struct mqtt_client *client, const struct mqtt_evt *evt);

void main(void) {
  printk("Hello World! %s\n", CONFIG_BOARD);

  if (ERROR(dk_buttons_init(button_handler)))
    printk("Error initializing buttons");

  int err = bt_enable(NULL);
  if (ERROR(err)) printk("Error enabling BT %d", err);
  bt_ready();

  struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;
  broker4->sin_family = AF_INET;
  broker4->sin_port = htons(SERVER_PORT);
  // net_addr_pton(AF_INET, SERVER_ADDR, &broker4->sin_addr);
  zsock_inet_pton(AF_INET, SERVER_ADDR, &broker4->sin_addr);

  uint8_t *client_id = "ZEPHYR_mqtt_client";
  mqtt_client_init(&client_ctx);
  client_ctx.broker = &broker;
  client_ctx.evt_cb = mqtt_evt_handler;
  client_ctx.client_id.utf8 = client_id;
  client_ctx.client_id.size = sizeof(client_id) - 1;
  client_ctx.password = NULL;
  client_ctx.user_name = NULL;
  client_ctx.protocol_version = MQTT_VERSION_3_1_1;
  client_ctx.transport.type = MQTT_TRANSPORT_NON_SECURE;

  client_ctx.rx_buf = rx_buffer;
  client_ctx.rx_buf_size = sizeof(rx_buffer);
  client_ctx.tx_buf = tx_buffer;
  client_ctx.tx_buf_size = sizeof(tx_buffer);

  int rc = mqtt_connect(&client_ctx);
  if (rc != 0) {
    LOG_ERR("ERROR: %d\n", rc);
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
    uint8_t *topic = "testtopic/zephyr";
    struct mqtt_publish_param param = {0};
    param.message.payload.data = message;
    param.message.payload.len = sizeof(message) - 1;
    param.message.topic.qos = MQTT_QOS_0_AT_MOST_ONCE;
    param.message.topic.topic.utf8 = topic;
    param.message.topic.topic.size = sizeof(topic) - 1;
    mqtt_publish(&client_ctx, &param);
  }

#ifndef DISABLE_NH3
  nrfx_spim_t instance = NRFX_SPIM_INSTANCE(0);
  nrfx_spim_config_t config = NRFX_SPIM_DEFAULT_CONFIG(
      SCK_PIN,
      NRFX_SPIM_PIN_NOT_USED,
      SDO_PIN, NRFX_SPIM_PIN_NOT_USED);
  config.mode = NRF_SPIM_MODE_3;
  nrf_gpio_cfg_output(CS_PIN);
  set_cs();
  if (nrfx_spim_init(&instance, &config, NULL, NULL) != NRFX_SUCCESS)
    printk("Could't enable spi\n");

  uint8_t rx_buff[3];
  struct nh3_spi_data data;
  nrfx_spim_xfer_desc_t transf = NRFX_SPIM_XFER_RX(rx_buff, 3);
#endif

  while (!quit && connected) {
#ifndef DISABLE_NH3
    int err = read_nh3_data(&instance, &transf);
    printk("ERROR_CODE: %d\n", err);
    if (err == NRFX_SUCCESS) {
      print_spi_data(rx_buff, sizeof(rx_buff));
      format_nh3_data(rx_buff, &data);
      print_nh3_struct(&data);
    }
#endif
    zsock_poll(fds, 1, 5000);
    mqtt_input(&client_ctx);
    mqtt_live(&client_ctx);
    k_sleep(K_MSEC(2000));
  }

  printk("QUIT\n");
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

#ifndef DISABLE_NH3
static inline void set_cs() {
  nrf_gpio_pin_set(CS_PIN);
  k_sleep(K_NSEC(100));
}

static inline void clear_cs() {
  nrf_gpio_pin_clear(CS_PIN);
  k_sleep(K_NSEC(100));
}

int read_nh3_data(nrfx_spim_t *instance, nrfx_spim_xfer_desc_t *transf) {
  uint32_t ready = 1;
  do {
    clear_cs();
    ready = nrf_gpio_pin_read(SDO_PIN);
    if (ready != 0) {
      set_cs();
      k_sleep(K_MSEC(10));
    }
    printk("0x%x\n", ready);
  } while (ready != 0);

  int err = nrfx_spim_xfer(instance, transf, 0);
  set_cs();
  return err;
}

void print_spi_data(uint8_t *spi_data, size_t size) {
  printk("DATA:");
  for (size_t i = 0; i < size; i++)
    printk(" %x", spi_data[i]);
  printk("\n");
}

void format_nh3_data(uint8_t *spi_data, struct nh3_spi_data *_out_data) {
  _out_data->OL = spi_data[0] >> 7;
  _out_data->OH = (spi_data[0] >> 6) & 0x01;
  _out_data->value = spi_data[2] | spi_data[1] << 8 | (spi_data[0] & 0x1f) << 16;
  _out_data->value |= ((spi_data[0] & 0x20) << 2) << 24;
}

void print_nh3_struct(struct nh3_spi_data *data) {
  printk("OL: %d OH: %d VALUE: %d", data->OL, data->OH, data->value);
}
#endif

void mqtt_evt_handler(struct mqtt_client *client, const struct mqtt_evt *evt) {
  switch (evt->type) {
    case MQTT_EVT_CONNACK:
      if (evt->result != 0) {
        LOG_ERR("Error connecting: %d\n", evt->result);
        break;
      }
      LOG_INF("MQTT connected\n");
      connected = true;
      break;
    case MQTT_EVT_DISCONNECT:
      LOG_INF("MQTT disconnected\n");
      connected = false;
      break;
    case MQTT_EVT_PINGRESP:
      LOG_INF("Ping response");
      break;
    default:
      break;
  }
}
