/*
 * Copyright (c) 2023 OAMK Corosect-project.
 *
 * SPDX-License-Identifier: MIT
 */

#include <dk_buttons_and_leds.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/pm.h>

#include "bluetooth.h"
#include "power.h"
#include "temp.h"

#ifndef CONFIG_DISABLE_NH3
#include "nh3_sensor.h"
#endif

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

#define ERROR(err) (err < 0)

void button_handler(uint32_t state, uint32_t has_changed);

volatile bool quit = false;
static struct k_sem sleep_sem;

void main(void) {
  LOG_INF("Hello World! %s", CONFIG_BOARD);

  // init(&sleep_sem);

  if (ERROR(dk_buttons_init(button_handler)))
    LOG_ERR("Error initializing buttons");

    // init_temp();
    // start_bt();

#ifndef CONFIG_DISABLE_NH3
  init_nh3();
#endif
  start_bt();

  while (!quit) {
#ifndef CONFIG_DISABLE_NH3
    read_and_print_nh3();
#endif

    // Initialize
    init_temp();
    start_bt();

    // Read temp and wait for 2 seconds for bt;
    int32_t temp = read_temp();
    set_temp(temp);
    send_data();

    // Got to sleep
    uninit_temp();
    stop_bt();
    k_sleep(K_SECONDS(10));
  }

  LOG_INF("QUIT\n");
  stop_bt();
}

void button_handler(uint32_t state, uint32_t has_changed) {
  if (DK_BTN2_MSK == has_changed) {
    quit = true;
    k_sem_give(&sleep_sem);
  }
}
