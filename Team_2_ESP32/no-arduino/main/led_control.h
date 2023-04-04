#ifndef _LED_CONTROL_H
#define _LED_CONTROL_H

#include "led_control.h"

/* Configure LED pin
 * set in menuconfig or source file */
void configure_led(void);

/* Set the LED color to values (0-255)
 * r: red
 * g: green
 * b: blue
 */
void set_led_color(int r, int g, int b);

/* Clear LED to turn it off */
void clear_led(void);

#endif //_LED_CONTROL_H
