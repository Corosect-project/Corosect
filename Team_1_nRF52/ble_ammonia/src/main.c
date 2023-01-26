/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dk_buttons_and_leds.h>
#include <nrf52.h>
#include <nrfx_spi.h>
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
}

static inline void clear_cs() {
  nrf_gpio_pin_clear(CS_PIN);
}

static inline void clock_pulse() {
  nrf_gpio_pin_clear(SCK_PIN);
  k_sleep(K_MSEC(1));
  nrf_gpio_pin_set(SCK_PIN);
  k_sleep(K_MSEC(1));
}

int read_an3_data(nrfx_spi_t *instance, nrfx_spi_xfer_desc_t *transf) {
  uint32_t ready = 1;
  uint32_t res = 0;
  do {
    clear_cs();
    ready = nrf_gpio_pin_read(SDO_PIN);
    if (ready != 0) k_sleep(K_MSEC(250));
    set_cs();
    printk("0x%x\n", ready);
  } while (ready != 0);

  clear_cs();
  k_sleep(K_MSEC(1));
    struct
    {
      uint8_t OL : 1;
      uint8_t OH : 1;
      uint8_t SIGN : 1;
    } flags;

    for (size_t i = 0; i < 24; i++) {
      nrf_gpio_pin_clear(SCK_PIN);
      k_sleep(K_MSEC(1));
      nrf_gpio_pin_set(SCK_PIN);
      switch (i) {
        case 0:
          flags.OL = nrf_gpio_pin_read(SDO_PIN);
          break;
        case 1:
          flags.OH = nrf_gpio_pin_read(SDO_PIN);
          break;
        case 2:
          flags.SIGN = nrf_gpio_pin_read(SDO_PIN);
          break;
        default:
          res <<= 1;
          res |= nrf_gpio_pin_read(SDO_PIN);
          break;
      }
      k_sleep(K_MSEC(1));
    }
  // int err = nrfx_spi_xfer(instance, transf, 0);
  set_cs();
  printk("%d %d %d\n", flags.OL, flags.OH, flags.SIGN);
  return res;
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

  nrfx_spi_t instance = NRFX_SPI_INSTANCE(0);
  nrfx_spi_config_t config = NRFX_SPI_DEFAULT_CONFIG(SCK_PIN, NRFX_SPI_PIN_NOT_USED, SDO_PIN, NRFX_SPI_PIN_NOT_USED);
  config.mode = NRF_SPI_MODE_3;
  config.frequency = SPI_FREQUENCY_FREQUENCY_K125;
  nrf_gpio_cfg_output(CS_PIN);
  nrf_gpio_cfg_input(SDO_PIN, NRF_GPIO_PIN_PULLUP);
  nrf_gpio_cfg_output(SCK_PIN);
  set_cs();
  nrf_gpio_pin_set(SCK_PIN);
  // nrfx_spi_init(&instance, &config, NULL, NULL);

  uint8_t rx_buff[3];
  nrfx_spi_xfer_desc_t transf = NRFX_SPI_XFER_RX(rx_buff, 3);

  // clear_cs();
  // for (size_t i = 0; i < 24; i++) clock_pulse();
  // set_cs();
  // k_sleep(K_MSEC(2000));
  // clear_cs();
  // k_sleep(K_MSEC(1));
  // set_cs();

  while (!quit) {
    int err = read_an3_data(&instance, &transf);
    printk("ERROR_CODE: %d\n", err);
    if (err == NRFX_SUCCESS)
      printk("DATA: 0x%x 0x%x 0x%x\n", rx_buff[0], rx_buff[1], rx_buff[2]);
    // // dk_set_leds(DK_ALL_LEDS_MSK);
    // // for (size_t i = 0; i < 4; i++) {
    // //   k_sleep(K_MSEC(1000));
    // //   dk_set_led(i, 0);
    // // }
    // clear_cs();
    // k_sleep(K_MSEC(2000));
    // set_cs();
    k_sleep(K_MSEC(2000));
  }
  printk("QUIT\n");
  bt_le_adv_stop();
  bt_disable();
}
