#include "bluetooth.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(app, LOG_LEVEL_DBG);

#define ERROR(err) (err < 0)

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(0xefaa)),
};

void bt_ready();
static void bt_connected(struct bt_conn *conn, uint8_t err);
static void bt_disconnected(struct bt_conn *conn, uint8_t reason);

BT_CONN_CB_DEFINE(con_calbacks) = {
    .connected = bt_connected,
    .disconnected = bt_disconnected};

int start_bt() {
  int err = bt_enable(NULL);
  if (ERROR(err)) {
    LOG_ERR("Error enabling BT %d", errno);
    return err;
  } else
    bt_ready();
}

void stop_bt() {
  bt_le_adv_stop();
  bt_disable();
}

void bt_ready() {
  int err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), 0, 0);
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
  }
}

void bt_disconnected(struct bt_conn *conn, uint8_t reason) {
  LOG_INF("Disconnected, reason %d", reason);
}
