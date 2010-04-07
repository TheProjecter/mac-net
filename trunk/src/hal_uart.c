/****************************************************************
 * file:	hal_uart.c
 * date:	2009-03-12
 * description:	uart support functions for CC2430
 ***************************************************************/
#include "ioCC2430.h"
#include "hal_uart.h"
#include "hal.h"

#define IO_PER_LOC_UART0_AT_PORT0_PIN2345() do { PERCFG = (PERCFG&~0x01)|0x00; } while (0)

// Support data are less than UART_RX_BUFSIZE
volatile UINT8 uart_rxbuf[UART_RX_BUFSIZE];
 volatile UINT16 rxbuf_head;
 UINT16 rxbuf_tail;

void uart_init()
{
	rxbuf_tail = 0;
	rxbuf_head = 0;	// reveive buff reset to 0;

	IO_PER_LOC_UART0_AT_PORT0_PIN2345();	// uart0 pin setup
	UTX0IF = 1;		// USART0 TX interrupt flag, set to interrupt pending, I don't why!
	URX0IE = 1;		// enable USART0 RX interrupt enable
	UART_SET_BAUD(9600);	// 9600..., 8N1, 115200bps 8 N1
}

BOOL uart_getch_rdy()
{
	UINT8	c;
	c = rxbuf_head;
	return (rxbuf_tail != c);

}

// get a character from uart0 (buffer), interrupt driven
UINT8 uart_getch()
{
	UINT16 c;
	do
	{
		c = rxbuf_head;	// use tmp because volatile declaration
	} while (rxbuf_tail == c);
	rxbuf_tail++;
	if (rxbuf_tail >= UART_RX_BUFSIZE)
		rxbuf_tail = 0;
	return (uart_rxbuf[rxbuf_tail]);
}

void uart_putch(UINT8 c)
{
	while (!UTX0IF)
		;
	UTX0IF = 0;
	U0DBUF = c;
}
void uart_puthex (UINT8 x) {
  UINT8 c;
  c = (x>>4)& 0xf;
  if (c > 9) uart_putch('A'+c-10);
   else uart_putch('0'+c);
  //LSDigit
  c = x & 0xf;
  if (c > 9) uart_putch('A'+c-10);
   else uart_putch('0'+c);
}


void uart_putstr(const char * str)
{
	while(*str != '\0')
		uart_putch(*str++);
}



// uart rx service interrupt routine
// warning: if the incoming characters are too much, it will
// 	override the former data.
// solution: use flow control, or increase the rx buf. not done yet !
#pragma vector=URX0_VECTOR
__interrupt static void urx0_isr()
{
	UINT16 x, y;
	UINT8 c;


	c = U0DBUF;
	x = rxbuf_head + 1;
	y = rxbuf_tail;

	if (x >= UART_RX_BUFSIZE)
		x = 0;

	if (x == y)
		return;	// full

	uart_rxbuf[x] = c;
	rxbuf_head = x;

}

