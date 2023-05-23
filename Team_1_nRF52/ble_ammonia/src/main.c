/*
 * Copyright (c) 2023 OAMK Corosect-project.
 *
 * SPDX-License-Identifier: MIT
 */

#include <dk_buttons_and_leds.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bluetooth.h"
#include "temp.h"

#define DISABLE_NH3
#ifndef DISABLE_NH3
#include "nh3_sensor.h"
#endif  // DISABLE_NH3

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

#define ERROR(err) (err < 0)

void button_handler(uint32_t state, uint32_t has_changed);

volatile bool quit = false;

void main(void) {
  LOG_INF("Hello World! %s", CONFIG_BOARD);

  if (ERROR(dk_buttons_init(button_handler)))
    LOG_ERR("Error initializing buttons");

  init_temp();
  start_bt();

#ifndef DISABLE_NH3
  init_nh3();
#endif

  while (!quit) {
#ifndef DISABLE_NH3
    read_and_print_nh3();
#endif
    int32_t temp = read_temp();
    // start_bt();
    set_temp(temp);
    k_sleep(K_SECONDS(2));
    // stop_bt();
    // k_sleep(K_SECONDS(30));
  }

  LOG_INF("QUIT\n");
  uninit_temp();
  stop_bt();
}

void button_handler(uint32_t state, uint32_t has_changed) {
  if (DK_BTN2_MSK == has_changed) {
    quit = true;
  }
}
