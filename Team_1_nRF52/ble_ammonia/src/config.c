#include <zephyr/logging/log.h>
#include <zephyr/net/net_config.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>

LOG_MODULE_DECLARE(main_log, LOG_LEVEL_DBG);

void cb(struct net_if* iface, void* user_data) {
  bool state = net_if_flag_is_set(iface, NET_IF_UP);
  LOG_DBG("Interface: %d, NET_IF_UP: %d", (*(size_t*)user_data)++, state);
}

static struct in6_addr my_addr;

size_t index = 0;
void init_network() {
  int32_t err;

  struct net_if *iface = net_if_get_default();
  if (!iface) {
    LOG_ERR("Didn't get interface");
    return;
  }

  net_addr_pton(AF_INET6, CONFIG_NET_CONFIG_MY_IPV6_ADDR, &my_addr);
  net_if_ipv6_addr_add(iface, &my_addr, NET_ADDR_MANUAL, 0);

  net_if_foreach(cb, &index);
  err = net_config_init_by_iface(iface, "Zephyr MQTT", NET_CONFIG_NEED_IPV6, 5000);
  if (err < 0) LOG_ERR("Initialization error, %d", err);
}