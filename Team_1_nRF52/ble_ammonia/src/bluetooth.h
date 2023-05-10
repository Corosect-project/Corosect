#ifndef D03F4BBC_6443_4B62_AD6C_C08762252757
#define D03F4BBC_6443_4B62_AD6C_C08762252757

#include <zephyr/kernel.h>

#define CUSTOM_UUID_VAL BT_UUID_128_ENCODE(0xd8b807ec, 0xef6e, 0x11ed, 0xa05b, 0x0242ac120003)  // d8b807ec-ef6e-11ed-a05b-0242ac120003
#define CUSTOM_UUID BT_UUID_DECLARE_128(CUSTOM_UUID_VAL)

int start_bt();
void stop_bt();
void set_temp(int32_t temp);

#endif /* D03F4BBC_6443_4B62_AD6C_C08762252757 */
