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
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main_log, LOG_LEVEL_DBG);

#define DISABLE_NH3
#ifndef DISABLE_NH3
#include "nh3_sensor.h"
#endif

#define ERROR(err) (err < 0)

volatile bool quit = false;

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xaa, 0xef),
};

void button_handler(uint32_t state, uint32_t has_changed);
void bt_ready();

void main(void) {
  LOG_INF("Hello World! %s", CONFIG_BOARD);

  if (ERROR(dk_buttons_init(button_handler)))
    printk("Error initializing buttons");

  int err = bt_enable(NULL);
  if (ERROR(err)) LOG_ERR("Error enabling BT %d", err);
  bt_ready();

#ifndef DISABLE_NH3
  init_nh3();
#endif

  while (!quit) {
#ifndef DISABLE_NH3
    read_and_print_nh3();
#endif
    k_sleep(K_MSEC(2000));
  }

  LOG_INF("QUIT\n");
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
