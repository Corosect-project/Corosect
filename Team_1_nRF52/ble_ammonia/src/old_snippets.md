# read_nh3_data

```c
 uint32_t res = 0;
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

printk("%d %d %d\n", flags.OL, flags.OH, flags.SIGN);
```

# main

```c
if (ERROR(dk_leds_init()))
  printk("Error initializing leds");
if (ERROR(dk_buttons_init(button_handler)))
  printk("Error initializing buttons");


nrf_gpio_cfg_input(SDO_PIN, NRF_GPIO_PIN_PULLUP);
nrf_gpio_cfg_output(SCK_PIN);

while (!quit) {
  dk_set_leds(DK_ALL_LEDS_MSK);
  for (size_t i = 0; i < 4; i++) {
    k_sleep(K_MSEC(1000));
    dk_set_led(i, 0);
  }
  clear_cs();
  k_sleep(K_MSEC(2000));
  set_cs();
    if (err == NRFX_SUCCESS) {
      int32_t result = (int32_t)rx_buff[2] | ((int32_t)rx_buff[1] << 8) | ((0x1f & (int32_t)rx_buff[0]) << 16);
      result |= (((int32_t)rx_buff[0] & 0x20) << 2) << 24;
      printk("DATA: 0x%x 0x%x 0x%x\nDEC: %d\n", rx_buff[0], rx_buff[1], rx_buff[2], result);
    }
}
```
