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
#include <zephyr/sys/byteorder.h>

LOG_MODULE_DECLARE(app, LOG_LEVEL_DBG);

#define ERROR(err) (err < 0)

ssize_t read_gatt(
    struct bt_conn *conn,
    const struct bt_gatt_attr *attr,
    void *buf,
    uint16_t len,
    uint16_t offset
);

static int32_t temp = 0.0f;

BT_GATT_SERVICE_DEFINE(
    sensor_service,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS),
    BT_GATT_CHARACTERISTIC(
        BT_UUID_TEMPERATURE,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_BROADCAST,
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

int start_bt() {
  int err = bt_enable(NULL);
  if (ERROR(err)) {
    LOG_ERR("Error enabling BT %d", errno);
    return err;
  }

  return 0;
}

void stop_bt() {
  // bt_le_adv_stop();
  bt_disable();
}

void set_temp(int32_t newTemp) {
  temp = newTemp;
  struct bt_gatt_attr *attr =
      bt_gatt_find_by_uuid(&sensor_service.attrs[0], 0, BT_UUID_TEMPERATURE);
  LOG_DBG("%p: %d", (void *)attr, *(int *)attr->user_data);
}

// clang-format off
#define INT_TO_LE_BYTE_ARRAY(val32) { \
      val32 & 0x000000ff,             \
      (val32 >> 8) & 0x000000ff,      \
      (val32 >> 16) & 0x000000ff,     \
      (val32 >> 24) & 0x000000ff,     \
  }
// clang-format on
#define SPREAD_BYTE_4(data) data[0], data[1], data[2], data[3]

void send_data() {
  struct bt_gatt_attr *attr =
      bt_gatt_find_by_uuid(&sensor_service.attrs[0], 0, BT_UUID_TEMPERATURE);
  if (!attr) {
    LOG_ERR("Error getting gatt attribute");
    return;
  }

  const void *data = attr->user_data;
  const uint32_t value = *(uint32_t *)data;
  const uint8_t fdata[] = INT_TO_LE_BYTE_ARRAY(value);

  LOG_DBG("Data: %x", value);
  LOG_DBG("%x %x %x %x", SPREAD_BYTE_4(fdata));

  struct bt_data ad[] = {
      BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_NO_BREDR)),
      BT_DATA_BYTES(
          BT_DATA_SVC_DATA16,
          BT_UUID_16_ENCODE(BT_UUID_ESS_VAL),
          SPREAD_BYTE_4(fdata)
      ),
  };
  LOG_DBG("%p: %d", (void *)attr, *(uint32_t *)data);

  int err = bt_le_adv_start(BT_LE_ADV_NCONN_NAME, ad, ARRAY_SIZE(ad), 0, 0);
  if (err == -ENOMEM) LOG_ERR("ENOMEM\n");
  else if (err == -ECONNREFUSED || err == -EIO) LOG_ERR("ECONNREFUSED\n");

  LOG_DBG("Error %d\n", err);
  k_sleep(K_SECONDS(1));
  bt_le_adv_stop();
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
