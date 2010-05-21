/****************************************************************
 * file:	hal_uart.h
 * date:	2010-05-21
 * description:	uart support functions for CC2430
 ***************************************************************/
#ifndef HAL_UART_H
#define	HAL_UART_H

#include "types.h"

#define UART_RX_BUFSIZE	480

// UART API, invoked by HAL layer
/************************************************
 	**	External Functions	**
 ***********************************************/
extern void uart_init();
extern UINT8 uart_getch_blocked();
extern UINT8 uart_rdy_getch();
extern void uart_putch(UINT8 c);
extern void uart_putstr(const char * str);
extern void uart_puthex (UINT8 x);
extern void uart_move_tail(UINT16 index);
extern void uart_flush_all();
extern INT32 uart_first_indexof(UINT8 ch);
extern UINT16 uart_unread_bytes(UINT16 base);
extern BOOL uart_buffer_full();
extern BOOL uart_buffer_empty();

// The macros in this section simplify UART operation.
#define HIGH_STOP                   0x02
#define TRANSFER_MSB_FIRST          0x80
#define UART_ENABLE_RECEIVE         0x40

#define BAUD_E(baud, clkDivPow) (     \
    (baud==2400)   ?  6  +clkDivPow : \
    (baud==4800)   ?  7  +clkDivPow : \
    (baud==9600)   ?  8  +clkDivPow : \
    (baud==14400)  ?  8  +clkDivPow : \
    (baud==19200)  ?  9  +clkDivPow : \
    (baud==28800)  ?  9  +clkDivPow : \
    (baud==38400)  ?  10 +clkDivPow : \
    (baud==57600)  ?  10 +clkDivPow : \
    (baud==76800)  ?  11 +clkDivPow : \
    (baud==115200) ?  11 +clkDivPow : \
    (baud==153600) ?  12 +clkDivPow : \
    (baud==230400) ?  12 +clkDivPow : \
    (baud==307200) ?  13 +clkDivPow : \
    0  )

#define BAUD_M(baud) (      \
    (baud==2400)   ?  59  : \
    (baud==4800)   ?  59  : \
    (baud==9600)   ?  59  : \
    (baud==14400)  ?  216 : \
    (baud==19200)  ?  59  : \
    (baud==28800)  ?  216 : \
    (baud==38400)  ?  59  : \
    (baud==57600)  ?  216 : \
    (baud==76800)  ?  59  : \
    (baud==115200) ?  216 : \
    (baud==153600) ?  59  : \
    (baud==230400) ?  216 : \
    (baud==307200) ?  59  : \
  0)

#define UART_SETUP(uart, baudRate, options)      \
   do {                                          \
      if((uart) == 0){                           \
         if(PERCFG & 0x01){                      \
            P1SEL |= 0x30;                       \
         } else {                                \
            P0SEL |= 0x0C;                       \
         }                                       \
      }                                          \
      else {                                     \
         if(PERCFG & 0x02){                      \
            P1SEL |= 0xC0;                       \
         } else {                                \
            P0SEL |= 0x30;                       \
         }                                       \
      }                                          \
                                                 \
      U##uart##GCR = BAUD_E((baudRate),CLKSPD);  \
      U##uart##BAUD = BAUD_M(baudRate);          \
                                                 \
      U##uart##CSR |= 0x80;                      \
                                                 \
                                                 \
      U##uart##UCR |= ((options) | 0x80);        \
                                                 \
      if((options) & TRANSFER_MSB_FIRST){        \
         U##uart##GCR |= 0x20;                   \
      }                                          \
      U##uart##CSR |= 0x40;                      \
   } while(0)

// Macro to set the baud rate
#define UART_SET_BAUD(baud) UART_SETUP(0, baud, HIGH_STOP);

#endif

