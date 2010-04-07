/****************************************************************
 * file:	hal_timer.c
 * date:	2009-03-13
 * description:	timer2 operations for CC2430
 ***************************************************************/

#include "hal_timer.h"

void mac_timer_init()
{
	T2CNF = 0x00;		// ensure timer is idle
	T2CAPHPH = 0x02;	// setting for 16 u-second periods
	T2CAPLPL = 0x00;	// (0x0200) / 32 = 16 u-seconds

	//set the interrupt compare to its maximum value	
	T2PEROF0 = 0xFF & (T2CMPVAL);
	T2PEROF1 = (UINT8) (T2CMPVAL>>8);
	//enable overflow count compare interrupt
	T2PEROF2 = ((UINT8) (T2CMPVAL>>16)) | 0x20;

	T2CNF = 0x03; 		// start timer
	T2IF = 0;		// clear interrupt flag
	T2IE = 1;		// enable interrupt
}

UINT32 mac_timer_get()
{
	UINT32 t;
	BOOL gie_status;

	SAVE_AND_DISABLE_GLOBAL_INTERRUPT(gie_status);
	t = 0x0FF & T2OF0;
	t += (((UINT16)T2OF1)<<8);
	t += (((UINT32) T2OF2 & 0x0F)<<16);
	RESTORE_GLOBAL_INTERRUPT(gie_status);

	return t;
}

UINT32 mac_ticks_to_us(UINT32 ticks)
{
	UINT32 retval;

	retval = (ticks/SYMBOLS_PER_MAC_TICK())* (1000000/LRWPAN_SYMBOLS_PER_SECOND);
	return retval;
}


/*
 * may have bug, ms should be converted to ticks first
 */
void mac_delay_time_ms(UINT32 delay_ms)
{
	UINT32 start_time, cur_time;

	start_time = mac_timer_get();
	do
	{
		cur_time = mac_timer_get();
	} while( ((cur_time - start_time)&MACTIMER_MAX_VALUE) < delay_ms );
}

//This timer interrupt is the periodic interrupt for evboard functions
#pragma vector=T2_VECTOR
__interrupt static void t2_isr()
{
	UINT32 t;

	INT_GLOBAL_ENABLE(INT_OFF);
	T2IF = 0;		// clear interrupt flag

	//compute next compare value by reading current timer value, adding offset
	t = 0x0FF & T2OF0;
	t += (((UINT16)T2OF1)<<8);
	t += (((UINT32) T2OF2 & 0x0F)<<16);
	t += T2CMPVAL;		// add offset
	T2PEROF0 = t & 0xFF;
	T2PEROF1 = (t >> 8) & 0xFF;
	//enable overflow count compare interrupt
	T2PEROF2 = ((t >> 16)&0x0F) | 0x20;
	T2CNF = 0x03; 		// this clears the timer2 flags

	INT_GLOBAL_ENABLE(INT_ON);
}

