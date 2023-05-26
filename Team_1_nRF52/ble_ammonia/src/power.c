#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(app, LOG_LEVEL_DBG);

#define ALARM_CHAN_ID 0
#define RTC DT_NODELABEL(rtc2)

static struct counter_alarm_cfg config;

static void alarm_cb(
    const struct device *dev, uint8_t chan_id, uint32_t ticks, void *user_data
) {
  int32_t now_ticks;
  int err = counter_get_value(dev, &now_ticks);
  if (err < 0) LOG_ERR("Error reading value");

  uint64_t now_usec = counter_ticks_to_us(dev, now_ticks);
  int now_sec = now_usec / USEC_PER_SEC;

  LOG_INF("!!! ALARM !!!");
  LOG_INF("Time  %u (%u ticks)", now_sec, now_ticks);

  struct k_sem *sem = user_data;
  k_sem_give(sem);
}

void init(struct k_sem *sleep_sem) {
  k_sem_init(sleep_sem, 1, 1);
  const struct device *rtc = DEVICE_DT_GET(RTC);
  if (!device_is_ready(rtc)) {
    LOG_ERR("Rtc not available");
    rtc = NULL;
    return;
  }

  int err = counter_start(rtc);
  if (err < 0) {
    LOG_ERR("Error: %d", -err);
    rtc = NULL;
    return;
  }

  LOG_DBG("Rtc started");

  config.flags = 0;
  config.ticks = counter_us_to_ticks(rtc, 30 * USEC_PER_SEC);
  config.callback = alarm_cb;
  config.user_data = sleep_sem;
}

void wait_for_rtc(struct k_sem *sleep_sem) {
  const struct device *rtc = DEVICE_DT_GET(RTC);
  if (config.user_data != sleep_sem) return;
  int err = counter_set_channel_alarm(rtc, ALARM_CHAN_ID, &config);
  if (err == -EBUSY) {
    LOG_WRN("EBUSY");
  } else if (err < 0) {
    LOG_ERR("Error while setting alarm: %d", -err);
    return;
  }
  k_sem_take(sleep_sem, K_FOREVER);
}
