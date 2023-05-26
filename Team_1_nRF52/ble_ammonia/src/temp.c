#include "temp.h"

#include <nrfx_temp.h>

static nrfx_temp_config_t config = NRFX_TEMP_DEFAULT_CONFIG;

void init_temp() { nrfx_temp_init(&config, NULL); }

void uninit_temp() { nrfx_temp_uninit(); }

int32_t read_temp() {
  int res = nrfx_temp_measure();
  int32_t raw_temp = 0;
  if (res == NRFX_SUCCESS) raw_temp = nrfx_temp_result_get();

  return nrfx_temp_calculate(raw_temp);
}
