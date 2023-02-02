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

#define ERROR(err) (err < 0)
#define CS_PIN 27
#define SCK_PIN 26
#define SDO_PIN 2

volatile bool quit = false;

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

static inline void set_cs() {
  nrf_gpio_pin_set(CS_PIN);
  k_sleep(K_NSEC(100));
}

static inline void clear_cs() {
  nrf_gpio_pin_clear(CS_PIN);
  k_sleep(K_NSEC(100));
}

int read_ah3_data(nrfx_spim_t *instance, nrfx_spim_xfer_desc_t *transf) {
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

void main(void) {
  printk("Hello World! %s\n", CONFIG_BOARD);

  int err = bt_enable(NULL);
  if (ERROR(err)) printk("Error enabling BT %d", err);
  bt_ready();

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
  nrfx_spim_xfer_desc_t transf = NRFX_SPIM_XFER_RX(rx_buff, 3);

  while (!quit) {
    int err = read_an3_data(&instance, &transf);
    printk("ERROR_CODE: %d\n", err);
    if (err == NRFX_SUCCESS) {
      int32_t result = (int32_t)rx_buff[2] | ((int32_t)rx_buff[1] << 8) | ((0x1f & (int32_t)rx_buff[0]) << 16);
      result |= (((int32_t)rx_buff[0] & 0x20) << 2) << 24;
      printk("DATA: 0x%x 0x%x 0x%x\nDEC: %d\n", rx_buff[0], rx_buff[1], rx_buff[2], result);
    }

    k_sleep(K_MSEC(2000));
  }
  printk("QUIT\n");
  bt_le_adv_stop();
  bt_disable();
}
