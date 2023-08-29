#ifndef BFD61C63_F38B_4026_BC7B_953DAE8DE655
#define BFD61C63_F38B_4026_BC7B_953DAE8DE655

#include <zephyr/kernel.h>

void init(struct k_sem *sleep_sem);

void wait_for_rtc(struct k_sem *sleep_sem);

#endif /* BFD61C63_F38B_4026_BC7B_953DAE8DE655 */
