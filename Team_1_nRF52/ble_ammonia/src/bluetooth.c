/**
 * @brief File for controling bluetooth GATT services
 * and managing the bluetoot state
 *
 * @copyright Copyright (c) 2023 OAMK Corosect-project
 *
 */
#include "bluetooth.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(app, LOG_LEVEL_DBG);

#define ERROR(err) (err < 0)

void bt_ready();
ssize_t read_gatt(
    struct bt_conn *conn,
    const struct bt_gatt_attr *attr,
    void *buf,
    uint16_t len,
    uint16_t offset
);
static void bt_connected(struct bt_conn *conn, uint8_t err);
static void bt_disconnected(struct bt_conn *conn, uint8_t reason);

int32_t temp = 0.0f;
struct bt_conn *connection = NULL;

BT_GATT_SERVICE_DEFINE(
    sensor_service,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS),
    BT_GATT_CHARACTERISTIC(
        BT_UUID_TEMPERATURE,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_READ,
        read_gatt,
        NULL,
        &temp
    ),
    BT_GATT_CHARACTERISTIC(
        CUSTOM_UUID,
        BT_GATT_CHRC_READ,
        BT_GATT_PERM_READ,
        read_gatt,
        NULL,
        "Hello, world"
    ),
);

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_ESS_VAL)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, CUSTOM_UUID_VAL),
};

BT_CONN_CB_DEFINE(con_calbacks) = {
    .connected = bt_connected,
    .disconnected = bt_disconnected,
};

int start_bt() {
  int err = bt_enable(NULL);
  if (ERROR(err)) {
    LOG_ERR("Error enabling BT %d", errno);
    return err;
  } else bt_ready();
  return 0;
}

void stop_bt() {
  bt_le_adv_stop();
  bt_disable();
}

void set_temp(int32_t newTemp) {
  temp = newTemp;
  struct bt_gatt_attr *attr =
      bt_gatt_find_by_uuid(&sensor_service.attrs[0], 0, BT_UUID_TEMPERATURE);
  LOG_DBG("%p: %d", (void *)attr, *(int *)attr->user_data);
  if (attr) bt_gatt_notify(connection, attr, attr->user_data, sizeof(temp));
}

void bt_ready() {
  int err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), 0, 0);
  if (err == -ENOMEM) LOG_ERR("ENOMEM\n");
  else if (err == -ECONNREFUSED || err == -EIO) LOG_ERR("ECONNREFUSED\n");

  LOG_DBG("%d\n", err);
}

ssize_t read_gatt(
    struct bt_conn *conn,
    const struct bt_gatt_attr *attr,
    void *buf,
    uint16_t len,
    uint16_t offset
) {
  void *value = attr->user_data;
  return bt_gatt_attr_read(conn, attr, buf, len, offset, value, strlen(value));
}

void bt_connected(struct bt_conn *conn, uint8_t err) {
  if (err) {
    LOG_ERR("Connection failed, %d", err);
  } else {
    LOG_INF("Connected");
    connection = conn;
  }
}

void bt_disconnected(struct bt_conn *conn, uint8_t reason) {
  LOG_INF("Disconnected, reason %d", reason);
  connection = NULL;
}
