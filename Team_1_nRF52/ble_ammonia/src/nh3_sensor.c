/*
 * Copyright (c) 2023 OAMK Corosect-project.
 *
 * SPDX-License-Identifier: MIT
 */
#include "nh3_sensor.h"

#include <nrfx_spim.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(app, LOG_LEVEL_DBG);

/**
 * @brief Tries to read nh3 data from the spi
 *
 * @return 0 on success and a negative error code on error
 */
int read_nh3_data();

nrfx_spim_t instance = NRFX_SPIM_INSTANCE(0);
nrfx_spim_config_t config = NRFX_SPIM_DEFAULT_CONFIG(
    SCK_PIN, NRFX_SPIM_PIN_NOT_USED, SDO_PIN, NRFX_SPIM_PIN_NOT_USED
);

uint8_t rx_buff[3];
struct nh3_spi_data data;
nrfx_spim_xfer_desc_t transf = NRFX_SPIM_XFER_RX(rx_buff, 3);

static inline void set_cs() {
  nrf_gpio_pin_set(CS_PIN);
  k_sleep(K_NSEC(100));
}

static inline void clear_cs() {
  nrf_gpio_pin_clear(CS_PIN);
  k_sleep(K_NSEC(100));
}

void init_nh3() {
  config.mode = NRF_SPIM_MODE_3;
  nrf_gpio_cfg_output(CS_PIN);
  set_cs();
  if (nrfx_spim_init(&instance, &config, NULL, NULL) != NRFX_SUCCESS)
    LOG_ERR("Could't enable spi\n");
}

// First reads the data and if raed is successful
// foramat the data using helper functions for printing
void read_and_print_nh3() {
  int err = read_nh3_data();
  printk("ERROR_CODE: %d\n", err);
  if (err == NRFX_SUCCESS) {
    print_spi_data(rx_buff, sizeof(rx_buff));
    format_nh3_data(rx_buff, sizeof(rx_buff), &data);
    print_nh3_struct(&data);
  }
}

// To read the nh3 data the function first starts the conversion process
// and waits for it to complete after which it reads the data
int read_nh3_data() {
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

  int err = nrfx_spim_xfer(&instance, &transf, 0);
  set_cs();
  return err;
}

void print_spi_data(uint8_t *spi_data, size_t size) {
  printk("DATA:");
  for (size_t i = 0; i < size; i++) printk(" %x", spi_data[i]);
  printk("\n");
}

// Formats `spi_data` which is uint8_t array in MSB, that contains atleas three
// elements. All elements above three are not accounted.
void format_nh3_data(
    uint8_t *spi_data, size_t size, struct nh3_spi_data *_out_data
) {
  if (size < 3) return;
  _out_data->OL = spi_data[0] >> 7;  // Get the MSB which indicates overflow low
  _out_data->OH = (spi_data[0] >> 6) & 0x01;  // Get the overflow high bit
  _out_data->value = spi_data[2] | spi_data[1] << 8 |     // Get the value bits
                     (spi_data[0] & 0x1f) << 16;          // from the data
  _out_data->value |= ((spi_data[0] & 0x20) << 2) << 24;  // Get the sign bit
}

void print_nh3_struct(struct nh3_spi_data *data) {
  printk("OL: %d OH: %d VALUE: %d", data->OL, data->OH, data->value);
}
