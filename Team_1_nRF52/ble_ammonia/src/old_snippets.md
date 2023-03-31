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

# net context tcp

```c
NET_PKT_TX_SLAB_DEFINE(tx_pkt_slab, 8);
NET_PKT_DATA_POOL_DEFINE(data_pool, 16);
static uint8_t rx_buff[256];

static struct k_mem_slab *tx_tcp_pool() {
  return &tx_pkt_slab;
}

static struct k_mem_slab *data_tcp_pool() {
  return &data_pool;
}

static inline void pkt_sent(struct net_context *context, int status, void *user_data) {
  if (status >= 0) {
    LOG_DBG("Sent %d bytes", status);
  }
}

static void receive(struct net_context *context,
                    struct net_pkt *pkt,
                    union net_ip_header *ip_hdr,
                    union net_proto_header *proto_hdr,
                    int status, void *user_data) {
  if (!pkt) return;  // Packet is EOF returning.
  int len = net_pkt_remaining_data(pkt);
  net_pkt_read(pkt, rx_buff, len);
  LOG_HEXDUMP_DBG(rx_buff, len, "RECIEVED_DATA");
  (void)net_context_update_recv_wnd(context, len);
  net_pkt_unref(pkt);
}

void main() {
  struct net_context *context = {0};

  net_context_get(AF_INET6, SOCK_STREAM, IPPROTO_TCP, &context);
  net_context_setup_pools(context, tx_tcp_pool, data_tcp_pool);
  net_context_connect(context, &peer, sizeof(peer), NULL, K_FOREVER, NULL);
  net_context_send(context, "Kusi mutteri", sizeof("Kusi mutteri"), pkt_sent, K_NO_WAIT, NULL);

  net_context_recv(context, receive, K_FOREVER, NULL);
}
```
