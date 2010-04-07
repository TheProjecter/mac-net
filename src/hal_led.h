/****************************************************************
 * file:	hal_led.h
 * date:	2009-03-12
 * description:	this controls led on the board
 ***************************************************************/

#ifndef HAL_LED_H
#define HAL_LED_H

#include "hal.h"

#define LED_OFF 1
#define LED_ON  0

#define LED_RED          P0_0 //this is LED RED, (led 2) on the board
#define LED_GREEN          P0_1 //this is LED GREEN , (led 3) on the board

#define INIT_LED_RED()   do { LED_RED = LED_OFF; IO_DIR_PORT_PIN(0, 0, IO_OUT); P0SEL &= ~0x01;} while (0)
#define INIT_LED_GREEN()   do { LED_GREEN = LED_OFF; IO_DIR_PORT_PIN(0, 1, IO_OUT); P0SEL &= ~0x02;} while (0)

#define INIT_LED()		do { INIT_LED_RED(); INIT_LED_GREEN(); } while (0)

#define LED_RED_ON()  (LED_RED = LED_ON)
#define LED_GREEN_ON()  (LED_GREEN = LED_ON)

#define LED_RED_OFF()  (LED_RED = LED_OFF)
#define LED_GREEN_OFF()  (LED_GREEN = LED_OFF)

#define LED_RED_TOGGLE() (LED_RED = ~LED_RED)
#define LED_GREEN_TOGGLE() (LED_GREEN = ~LED_GREEN)

#define LED_RED_STATE() (LED_RED == LED_ON)
#define LED_GREEN_STATE() (LED_GREEN == LED_ON)

#endif

