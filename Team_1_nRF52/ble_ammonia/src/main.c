/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dk_buttons_and_leds.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/kernel.h>

volatile bool quit = false;

#define ERROR(err) (err < 0)

void button_handler(uint32_t state, uint32_t has_changed) {
  if (DK_BTN2_MSK == has_changed) {
    quit = true;
  }
}

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xaa, 0xef),
};

void bt_ready() {
  int err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), 0, 0);
  if (err == -ENOMEM)
    printk("ENOMEM\n");
  else if (err == -ECONNREFUSED || err == -EIO)
    printk("ECONNREFUSED\n");

  printk("%d\n", err);
}

void main(void) {
  printk("Hello World! %s\n", CONFIG_BOARD);
  if (ERROR(dk_leds_init()))
    printk("Error initializing leds");
  if (ERROR(dk_buttons_init(button_handler)))
    printk("Error initializing buttons");

  int err = bt_enable(NULL);
  if (ERROR(err)) printk("Error enabling BT %d", err);
  bt_ready();

  while (!quit) {
    dk_set_leds(DK_ALL_LEDS_MSK);
    for (size_t i = 0; i < 4; i++) {
      k_sleep(K_MSEC(1000));
      dk_set_led(i, 0);
    }
    k_sleep(K_MSEC(2000));
  }
  printk("QUIT\n");
  bt_le_adv_stop();
  bt_disable();
}
