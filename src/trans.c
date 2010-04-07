

#include "hal_uart.h"
#include "trans.h"
#include "hal.h"
#include "nwk.h"
#include "queue.h"
#include "mac.h"
#include "hal_uart.h"


#include "route_cache.h"

UINT16 frm_head;

BOOL frm_pending;
UINT8 frm_len;

void trans_debug();


void trans_init()
{
	
	frm_head = 0;
	frm_len = 0;
	frm_pending = FALSE;
}

BOOL trans_frm_avail()
{
	UINT16 i;
	UINT8 len;
	if (frm_pending == FALSE) {
		// search for a new start delimeter
		i = rxbuf_tail;
		while(i != rxbuf_head) {
			if (uart_rxbuf[i] == FRAME_DELIMETER) {
				frm_head = i;
				frm_pending = TRUE;
				break;
			}
			i++;
			if (i == UART_RX_BUFSIZE)
				i = 0;	// wrap around
		}
		return FALSE;
	}

	// now, we have frame head found
	if (frm_len == 0) {
		// search for frame len, just after the delimeter
		i = frm_head + 1;
		if (i == UART_RX_BUFSIZE)
			i = 0;
		if (i == rxbuf_head)
			return FALSE;	// len is not received yet
		frm_len = uart_rxbuf[i];

		if (frm_len > FRAME_MAX_LEN)	// invalid value, just set it to 4
			frm_len = 4;				// TODO: find a more elegant solution
	}

	// now, we have frame head and len found
	i = frm_head+1;
	if (i == UART_RX_BUFSIZE)
		i = 0;
	len = frm_len + 1;		// frame data len plus checksum, the remain bytes
	while (i != rxbuf_head) {
		i++;
		if (i == UART_RX_BUFSIZE)
			i = 0;
		len--;
		if (len == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

/*
 * parse one frame already in the uart buffer
 */
void trans_frm_parse()
{
	UINT8 cksum;
	UINT8 c;
	UINT8 len;
	UINT8 lsb;
	UINT8 msb;
	
	UINT8 para_type;
	UINT16 saddr;


	do {
		c = uart_getch();
	} while (c != FRAME_DELIMETER);

	len = uart_getch();

	if (len > FRAME_MAX_LEN)
		len = 4;				// TODO: find a more elegant solution


	cksum = len;
	c = uart_getch();	// frame type
	cksum += c;
	switch(c) {
		case FRAME_TYPE_TX_DATA:	// TODO: make a new function to handle it
			lsb = uart_getch();
			cksum += lsb;
			msb = uart_getch();
			cksum += msb;
			len -= 3;	// frame type + short addr, now len is payload length

			if (queue_is_full(&nwk_send_queue) || (len > QUEUE_ENTRY_LENGTH - sizeof ( nwk_opt_t))){
#ifdef STATISTIC				
				statistic_data_drop++;
#endif
				while (len > 0) {	// remove the data in UART buffer,TODO: optimize me
					uart_getch();
					len --;
				}
				uart_getch();	// the check sum in UART buffer
				break;	
			} else { // insert to send queue
				saddr = (msb << 8) + lsb;
				queue_add_element(&nwk_send_queue, len, saddr, &cksum);
				cksum += uart_getch();
				if (cksum != 0xff) {
					// now we remove the entry in the queue just inserted
					queue_remove_head(&nwk_send_queue);
				}
			}
			break;
		case FRAME_TYPE_GET_STATISTIC:
			cksum += uart_getch();
			if (cksum == 0xff)
				trans_put_statistic();
			break;

		case FRAME_TYPE_CLR_STATISTIC:
			cksum += uart_getch();
			if (cksum == 0xff)
				trans_clear_statistic();
			break;

		case FRAME_TYPE_GET_PARA:
			para_type = uart_getch();	// para type
			cksum += para_type;
	//		uart_putstr("para type: ");
//			uart_puthex(c);
			c = uart_getch();	// para value lsb
			cksum += c;
//			uart_puthex(c);
			c = uart_getch();	// para value msb
			cksum += c;
//			uart_puthex(c);		
			c = uart_getch();	// check sum
			cksum += c;
			if (cksum == 0xff) {

//			rt_dump();


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
						lsb = mac_pib.rx_rssi;
						msb = 0;
						break;
					case PARA_TYPE_TEMPERATURE:
						lsb = hal_get_temperature();	//  not verified yet
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
			para_type = uart_getch();	// para type
			cksum += para_type;
	//		uart_putstr("para type: ");
	//		uart_puthex(c);
			lsb = uart_getch();	// para value lsb
			cksum += lsb;
	//		uart_puthex(c);
			msb = uart_getch();	// para value msb
			cksum += msb;
	//			uart_puthex(c);		
			c = uart_getch();	// check sum
			cksum += c;
			if (cksum == 0xff) {
				switch(para_type) {
					case PARA_TYPE_TXPOWER:
						hal_set_RF_power(lsb);
						break;
					case PARA_TYPE_MYSADDR:
						// not implemented yet
						break;
					case PARA_TYPE_BAUDRATE:
						trans_set_baudrate(lsb);
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

// called when receive packet from other node
// ptr points to payload, len is the lenght of payload
void trans_fram_assemble(UINT16 src_addr, UINT8 *ptr, UINT8 len)
{
	UINT8 i;
	UINT8 c;
	UINT8 cksum;

#ifdef STATISTIC
	statistic_data_rx++;
#endif

/*
	uart_putstr("tr: ");
	uart_puthex(src_addr>>8);
	uart_puthex(src_addr & 0xff);
	uart_putch('\n');
	for (i = 0; i < len; i++)
		uart_putch(*(ptr+i));
	uart_putch('\r');
*/
	uart_putch(FRAME_DELIMETER);	// delimeter
	c = len + 3;		// frame type(1) + address(2)
	uart_putch(c);
	cksum = c;
	uart_putch(FRAME_TYPE_RX_DATA);		// we recieve data from other nodes, now send it to PC
	cksum += FRAME_TYPE_RX_DATA;
	c = src_addr & 0xff;	
	uart_putch(c);	// src address low byte
	cksum += c;
	c = src_addr >> 8;
	uart_putch(c);	// src address high byte
	cksum += c;

	for (i = 0; i < len; i++) {
		c = *(ptr+i);
		uart_putch(c);
		cksum += c;
	}
	cksum = 0xff -cksum;		// maybe   ~cksum is ok
	uart_putch(cksum);	// last byte, the check sum

}


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

void trans_set_baudrate(UINT8 val)
{
	switch(val) {
		case 0:
			UART_SET_BAUD(9600);
			break;
		case 1:
			UART_SET_BAUD(14400);
			break;
		case 2:
			UART_SET_BAUD(19200);
			break;
		case 3:
			UART_SET_BAUD(28800);
			break;
		case 4:
			UART_SET_BAUD(38400);
			break;
		case 5:
			UART_SET_BAUD(57600);
			break;
		case 6:
			UART_SET_BAUD(76800);
			break;
		case 7:
			UART_SET_BAUD(115200);
			break;
		default:
			break;
	}
}
