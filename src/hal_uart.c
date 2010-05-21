/****************************************************************
 * file:	hal_uart.c
 * date:	2010-05-21
 * description:	uart support functions for CC2430
 ***************************************************************/
#include "ioCC2430.h"
#include "hal_uart.h"
#include "hal.h"

#define IO_PER_LOC_UART0_AT_PORT0_PIN2345() do { PERCFG = (PERCFG&~0x01)|0x00; } while (0)

/************************************************
 	**	Local Variables		**
 ***********************************************/
// Support data are less than UART_RX_BUFSIZE
static UINT8 uart_rxbuf[UART_RX_BUFSIZE];
static UINT16 rxbuf_head;	// Writeable pointer index--Not full!
static UINT16 rxbuf_tail;	// Readable pointer index
static BOOL rxbuf_full;		// buff full?

void uart_init()
{
	rxbuf_head = 0;	// reveive buff reset to 0;
	rxbuf_tail = 0;
	rxbuf_full = FALSE;	// initialize to empty.

	IO_PER_LOC_UART0_AT_PORT0_PIN2345();	// uart0 pin setup
	UTX0IF = 1;		// USART0 TX interrupt flag, set to interrupt pending, I don't why!
	URX0IE = 1;		// enable USART0 RX interrupt enable
	UART_SET_BAUD(9600);	// 9600..., 8N1, 115200bps 8 N1
}

// get a character from uart0 (buffer), interrupt driven
// assume uart_buffer_empty is invoked first.
UINT8 uart_getch_blocked()
{
	UINT16 c;
	if (uart_buffer_empty())
	{
		do
		{
			c = rxbuf_head;	// use tmp because volatile declaration
		} while (rxbuf_tail == c);
	}
	if (rxbuf_tail +1 >= UART_RX_BUFSIZE)
		rxbuf_tail = 0;
	// Update rxbuf state
	if (rxbuf_full == TRUE)
		rxbuf_full = FALSE;
	return (uart_rxbuf[rxbuf_tail++]);
}

/************************************************
 * Description: Get next unread char with unblocked mode
 * 	Thus, we assume the invoker has known the next is
 * 	already ready! Otherwise, this action is undefine.
 * Arguments:
 * 	None.
 * Return:
 * 	UINT8--The next unread char.
 *
 * Date: 2010-05-19
 ***********************************************/
UINT8 uart_rdy_getch()
{
	UINT8 temp;

	temp = uart_rxbuf[rxbuf_tail++];
	// Round up
	if ( rxbuf_tail >= UART_RX_BUFSIZE )
		rxbuf_tail = 0;
	// Update state flag
	rxbuf_full = FALSE;

	return temp;
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

/************************************************
 * Description: Move the rxbuf_tail index to [index]
 * 	We assume the user has ensured rx_buf can read to [index]
 * Arguments:
 *	index -- The new rxbuf_tail pointer index.
 * Return:
 * 	None.
 *
 * Date: 2010-05-19
 ***********************************************/
void uart_move_tail(UINT16 index)
{
	// Move the read pointer index to [index]
	// So, the available byte is uart_rxbuf[index]
	rxbuf_tail = index;
	// Update state
	rxbuf_full = FALSE;
}

/************************************************
 * Description: Flush all the data in rxbuf to empty.
 * Arguments:
 * 	None.
 * Return:
 * 	None.
 *
 * Date: 2010-05-19
 ***********************************************/
void uart_flush_all()
{
	// Move the tail to head
	rxbuf_tail = rxbuf_head;
	// Update the state flag
	rxbuf_full = FALSE;
}

/************************************************
 * Description: Find the first index of ch in rxbuf[tail~head].
 * Arguments:
 * 	ch -- UINT8 searching char.
 * Return:
 * 	-1 -- If char not found in rxbuf[tail~head]
 * 	>=0 -- If found.
 *
 * Date: 2010-05-19
 ***********************************************/
INT32 uart_first_indexof(UINT8 ch)
{
	// Walk through
	UINT16 start = rxbuf_tail;
	// Not empty in uart_rxbuf
	if (uart_buffer_empty())
	{
		return -1;
	}

	do {
		if ( uart_rxbuf[start] == ch )
			break;

		if ( ++start == UART_RX_BUFSIZE )
			start = 0;
	}
	while( start != rxbuf_head );
	// If we find it
	if ( start == rxbuf_head )
		return -1;
	return start;
}

/************************************************
 * Description: How many bytes are unreaded from base to rxbuf_head?
 * Arguments:
 * 	base:
 * Return:
 * 	UINT16--The number of unread bytes from [base] to rxbuf_head.
 *
 * Date: 2010-05-19
 ************************************************/
UINT16 uart_unread_bytes(UINT16 base)
{
	return (rxbuf_head - base + UART_RX_BUFSIZE)%UART_RX_BUFSIZE;
}

/************************************************
 * Description: If buffer full.
 * Arguments:
 * 	None.
 * Return:
 * 	TRUE--If rx buffer is full
 * 	FALSE--Otherwise.
 *
 * Date: 2010-05-19
 ***********************************************/
BOOL uart_buffer_full()
{
	return rxbuf_full;
}

/************************************************
 * Description: If buffer is empty.
 * Arguments:
 * 	None.
 * Return:
 * 	TRUE--If rx buffer is full
 * 	FALSE--Otherwise.
 *
 * Date: 2010-05-19
 ***********************************************/
BOOL uart_buffer_empty()
{
	return (rxbuf_full == FALSE && rxbuf_head == rxbuf_tail);
}

// uart rx service interrupt routine
// warning: if the incoming characters are too much, it will
// 	override the former data.
// solution: use flow control, or increase the rx buf. not done yet !
#pragma vector=URX0_VECTOR
__interrupt static void urx0_isr()
{
	UINT16 x;
	UINT8 c;

	// If buffer full, read later or drop? don't know!
	if (rxbuf_full == TRUE)
		return;

	c = U0DBUF;
	x = rxbuf_head;

	uart_rxbuf[x++] = c;
	if (x >= UART_RX_BUFSIZE)
		x = 0;
	rxbuf_head = x;
	// Update buffer state
	if (rxbuf_head == rxbuf_tail && rxbuf_full == FALSE)
		rxbuf_full = TRUE;
}

