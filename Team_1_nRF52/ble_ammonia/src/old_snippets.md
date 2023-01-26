# read_an3_data

``` c
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

```` c
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
}
````