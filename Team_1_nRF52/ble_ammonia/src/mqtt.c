#include "mqtt.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(app, LOG_LEVEL_DBG);

// MQTT global variables
struct zsock_pollfd fds[1] = {0};
bool connected = false;

// MQTT
static uint8_t rx_buffer[256];
static uint8_t tx_buffer[256];
static struct mqtt_client client_ctx;
static struct sockaddr_storage broker;

void mqtt_evt_handler(struct mqtt_client *client, const struct mqtt_evt *evt);

struct mqtt_client *init_mqtt(const char *server_ip, uint32_t port) {
  uint32_t err;

  uint8_t *client_id = "ZEPHYR";
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

  struct sockaddr_in6 *broker6 = (struct sockaddr_in6 *)&broker;
  memset(broker6, 0, sizeof(*broker6));

  broker6->sin6_family = AF_INET6;
  broker6->sin6_port = htons(port);
  err = net_addr_pton(AF_INET6, server_ip, &broker6->sin6_addr);
  LOG_DBG("TEST: %d", err);
  // zsock_inet_pton(AF_INET6, SERVER_ADDR, &broker6->sin6_addr);

  fds[0].fd = client_ctx.transport.tcp.sock;
  fds[0].events = ZSOCK_POLLIN;

  if (err == 0)
    return &client_ctx;
  else
    return NULL;
}

uint32_t send_message(char *msg, char *topic) {
  struct mqtt_publish_param param = {0};
  param.message.payload.data = msg;
  param.message.payload.len = strlen(msg);
  param.message.topic.qos = MQTT_QOS_0_AT_MOST_ONCE;
  param.message.topic.topic.utf8 = topic;
  param.message.topic.topic.size = strlen(topic);
  return mqtt_publish(&client_ctx, &param);
}

void mqtt_evt_handler(struct mqtt_client *client, const struct mqtt_evt *evt) {
  LOG_ERR("MQTT_CALLBACK");
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
      LOG_INF("MQTT default");
      break;
  }
}
