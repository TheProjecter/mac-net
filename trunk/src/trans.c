/************************************************
 * File: trans.c
 * Description: Implement for transport layer.
 * Date: 2009-10-27
 ***********************************************/
#include "hal_uart.h"
#include "trans.h"
#include "hal.h"
#include "mac.h"
#include "queue.h"

static UINT16 frm_head;
static BOOL frm_pending;
static UINT8 frm_len;

#ifdef STATISTIC
static void trans_put_statistic();	// return statistic info to PC/ARM
static void trans_clear_statistic();
#endif

// The extra length for 'len' byte and 'checksum'  byte.
#define EXTRA_LEN_CHECKSUM 2

void trans_init()
{
	frm_head = 0;
	frm_len = 0;
	frm_pending = FALSE;
}

/************************************************
 * Description:	Detect if new frame is available?
 * Arguments:	None.
 * Return:	TRUE--if new frame is available.
 * 		FALSE--no new frame.
 * Date:	2010-04-10
 ************************************************/
BOOL trans_frm_avail()
{
	INT32 index;

	if (frm_pending == FALSE) {
		// search for a new start delimeter
		index = uart_first_indexof(FRAME_DELIMETER);
		if (index != -1) {
			frm_head = (UINT16)index;
			frm_pending = TRUE;
			// Move the read pointer index to the frame length.
			uart_move_tail((frm_head+1)%UART_RX_BUFSIZE);
		}
		else
		{
			if ( !uart_buffer_empty() )
				// Flush the invalid frame. since no valid delimeter is found through here.
				uart_flush_all();
			return FALSE;
		}
	}

	// now, we have frame head found
	if (frm_len == 0) {
		// If frame length byte is received?
		if (uart_unread_bytes(frm_head)> 1)
		{
			// after frame length is parameter type.
			frm_len = uart_rdy_getch();
			// If frm_len is invalid, no data included
			if (frm_len == 0) {
				frm_pending = FALSE;
				return FALSE;
			}
		}
		else
			return FALSE;
	}

	// now, we have frame head and len found
	// if the data in uart buffer can loaded in a frame(checksum included.)
	if (uart_unread_bytes(frm_head) > frm_len + EXTRA_LEN_CHECKSUM)
		return TRUE;
	return FALSE;
}

/************************************************
 * Description:	parse one frame already in the uart buffer
 * Arguments:	None.
 * Return:	None.
 * Date:	2009-11-27
 ************************************************/
void trans_frm_parse()
{
	UINT8 cksum;
	UINT8 c;
	UINT8 len;
	UINT8 lsb;
	UINT8 msb;
	
	UINT16 saddr;
	UINT8 para_type;

	// We assume that you have invoked trans_frm_avail before.
	// So, here we use the frm_head and frm_len without check.
	if (frm_pending == FALSE)
		return;
	
	// Parse data.
	len = frm_len;
	cksum = len;
	c = uart_rdy_getch();	// frame type
	cksum += c;
	switch(c) {
		case FRAME_TYPE_TX_DATA:
			lsb = uart_rdy_getch();
			cksum += lsb;
			msb = uart_rdy_getch();
			cksum += msb;
			len -= 3;	// frame type + short addr, now len is payload length
			if (queue_is_full(&mac_send_queue)){
#ifdef STATISTIC				
				statistic_data_drop++;
#endif
				// Just drop this frame, move rxbuffer's tail pointer index ahead
				// Since we now pointer to real data. so move [frm_head + len +1]
				// Including checksum.
				uart_move_tail(frm_head + len +1);
				break;	
			} else {
				// insert to send queue
				queue_add_element(&mac_send_queue, len, &cksum);
				cksum += uart_rdy_getch();
				if (cksum != 0xff) {
					// now we remove the entry in the queue just inserted
					queue_remove_head(&mac_send_queue);
				}
			}
			break;
#ifdef STATISTIC
		case FRAME_TYPE_GET_STATISTIC:
			// Hand the statistic information to PC/ARM
			cksum += uart_rdy_getch();
			if (cksum == 0xff)
				trans_put_statistic();
			break;

		case FRAME_TYPE_CLR_STATISTIC:
			// We clear the statistic information--do a RESET
			cksum += uart_rdy_getch();
			if (cksum == 0xff)
				trans_clear_statistic();
			break;
#endif
		case FRAME_TYPE_GET_PARA:
			para_type = uart_rdy_getch();	// para type
			cksum += para_type;
			c = uart_rdy_getch();		// para value lsb
			cksum += c;
			c = uart_rdy_getch();		// para value msb
			cksum += c;
			c = uart_rdy_getch();		// check sum
			cksum += c;
			if (cksum == 0xff) {
				// if chsum is ok , we send back parameter to PC
				uart_putch(FRAME_DELIMETER);
				uart_putch(4);
				uart_putch(FRAME_TYPE_RET_PARA);
				uart_putch(para_type);

				switch(para_type) {
					case PARA_TYPE_TXPOWER:
						lsb = hal_get_RF_power();
						msb = 0;	// we set msb manually to 0
						break;
					case PARA_TYPE_MYSADDR:
						saddr = mac_get_addr();
						lsb = saddr & 0xff;
						msb = (saddr >> 8) & 0xff;
						break;
					case PARA_TYPE_RSSI:
						lsb = mac_get_rssi();
						msb = 0;
						break;
				}

				uart_putch(lsb);
				uart_putch(msb);
				cksum = 4+FRAME_TYPE_RET_PARA+para_type+lsb+msb;
				uart_putch(0xff - cksum);
			}
			break;
		case FRAME_TYPE_SET_PARA:
			para_type = uart_rdy_getch();	// para type
			cksum += para_type;
			lsb = uart_rdy_getch();		// para value lsb
			cksum += lsb;
			msb = uart_rdy_getch();		// para value msb
			cksum += msb;
			c = uart_rdy_getch();		// check sum
			cksum += c;
			if (cksum == 0xff) {
				switch(para_type) {
					case PARA_TYPE_TXPOWER:
						hal_set_RF_power(lsb);
						break;
					case PARA_TYPE_MYSADDR:
						// TODO: not implemented yet
						break;
					default:
						break;
				}
			}
			break;	
		default:
			break;
	}	
	frm_pending = FALSE;
	frm_len = 0;
}

#ifdef STATISTIC
void trans_put_statistic()
{
	UINT8 cksum;
	UINT8 c;

	uart_putch(FRAME_DELIMETER);	// delimeter
	c = 1 + 14;		// len: type + seven two-byte variables
	uart_putch(c);	// len
	cksum = c;
	uart_putch(FRAME_TYPE_RET_STATISTIC);
	cksum += FRAME_TYPE_RET_STATISTIC;

	c = statistic_mac_tx & 0xff;
	uart_putch(c);
	cksum += c;
	c = statistic_mac_tx >> 8;
	uart_putch(c);
	cksum += c;

	c = statistic_mac_rx & 0xff;
	uart_putch(c);
	cksum += c;
	c = statistic_mac_rx >> 8;
	uart_putch(c);
	cksum += c;

	c = statistic_mac_drop & 0xff;
	uart_putch(c);
	cksum += c;
	c = statistic_mac_drop >> 8;
	uart_putch(c);
	cksum += c;

	c = statistic_mac_err & 0xff;
	uart_putch(c);
	cksum += c;
	c = statistic_mac_err >> 8;
	uart_putch(c);
	cksum += c;

	c = statistic_data_rx & 0xff;
	uart_putch(c);
	cksum += c;
	c = statistic_data_rx >> 8;
	uart_putch(c);
	cksum += c;

	c = statistic_data_tx & 0xff;
	uart_putch(c);
	cksum += c;
	c = statistic_data_tx >> 8;
	uart_putch(c);
	cksum += c;

	c = statistic_data_drop & 0xff;
	uart_putch(c);
	cksum += c;
	c = statistic_data_drop >> 8;
	uart_putch(c);
	cksum += c;

	cksum = 0xff -cksum;		// maybe   ~cksum is ok
	uart_putch(cksum);	// last byte, the check sum

}

void	trans_clear_statistic()
{
	statistic_mac_tx = 0;
	statistic_mac_rx = 0;
	statistic_mac_drop = 0;
	statistic_mac_err = 0;

	statistic_data_rx = 0;
	statistic_data_tx = 0;
	statistic_data_drop = 0;
}
#endif

