/****************************************************************
 * file:	hal_timer.h
 * date:	2009-03-13
 * description:	timer2 operations for CC2430
 ***************************************************************/

#ifndef HAL_TIMER_H
#define HAL_TIMER_H
#include "hal.h"

#define SLOWTICKS_PER_SECOND 10		// interrupt every 100ms
// assuming 16us period, have 1/16us = 62500 tics per seocnd
#define T2CMPVAL (62500/SLOWTICKS_PER_SECOND)

#define LRWPAN_SYMBOLS_PER_SECOND 62500
//Timer Support
//For the CC2430, we set the timer to exactly 1 tick per symbol
//assuming a clock frequency of 32MHZ, and 2.4GHz
#define SYMBOLS_PER_MAC_TICK()	1
#define SYMBOLS_TO_MACTICKS(x)	(x/SYMBOLS_PER_MAC_TICK())	//every
#define MSECS_TO_MACTICKS(x)	(x*(LRWPAN_SYMBOLS_PER_SECOND/1000))
#define MACTIMER_MAX_VALUE	0x000FFFFF	//20 bit counter, about 16 seconds max for 32MHZ crystal
#define MAC_TIMER_NOW_DELTA(x)	((mac_timer_get() - (x)) & MACTIMER_MAX_VALUE)

void mac_timer_init();
UINT32 mac_timer_get();
UINT32 mac_ticks_to_us(UINT32 ticks);
void mac_delay_time_ms(UINT32 delay_ms);

#endif

