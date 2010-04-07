/****************************************************************
 * file:	radio.h
 * date:	2009-03-13
 * description:	radio interface for CC2430
 ***************************************************************/

#ifndef RADIO_H
#define RADIO_H

#include "hal.h"

#define RADIO_DEFAULT_CHANNEL	20
#define RADIO_DEFAULT_PANID	0x1347
#define UNIT_BACKOFF_PERIOD	20
#define MAX_BE			5
#define MIN_BE			0
#define MAX_CSMA_BACKOFFS	4

LRWPAN_STATUS_ENUM radio_init();
LRWPAN_STATUS_ENUM radio_send_packet(UINT8 flen, UINT8 *frm);
static void radio_init_addr();
static LRWPAN_STATUS_ENUM radio_set_frequency(UINT8 channel);
// Callback functions

void phy_tx_start_callback(void);
void phy_tx_end_callBack(void);
void usrIntCallback();
// interrupt related stuff
#define INT_ENABLE_RF(on)	{ (on) ? (IEN2 |= 0x01) : (IEN2 &= ~0x01); }
#define INT_ENABLE_RFERR(on)	{ RFERRIE = on; }
#define INT_ENABLE_T2(on)	{ T2IE    = on; }
#define INT_ENABLE_URX0(on)	{ URX0IE  = on; }

#define INT_GETFLAG_RFERR()	RFERRIF
#define INT_GETFLAG_RF()	S1CON &= ~0x03

#define INT_SETFLAG_RFERR(f)	RFERRIF= f
#define INT_SETFLAG_RF(f)	{ (f) ? (S1CON |= 0x03) : (S1CON &= ~0x03); }

// marcos and definitions for radio interface
#define STOP_RADIO()        ISRFOFF;

// RF interrupt flags
#define IRQ_RREG_ON	0x80
#define IRQ_TXDONE	0x40
#define IRQ_FIFOP	0x20
#define IRQ_SFD		0x10
#define IRQ_CCA		0x08
#define IRQ_CSP_WT	0x04
#define IRQ_CSP_STOP	0x02
#define IRQ_CSP_INT	0x01

// RF status flags
#define TX_ACTIVE_FLAG	0x10
#define FIFO_FLAG	0x08
#define FIFOP_FLAG	0x04
#define SFD_FLAG	0x02
#define CCA_FLAG	0x01

// Radio status states
#define TX_ACTIVE	(RFSTATUS & TX_ACTIVE_FLAG)
#define FIFO		(RFSTATUS & FIFO_FLAG)
#define FIFOP		(RFSTATUS & FIFOP_FLAG)
#define SFD		(RFSTATUS & SFD_FLAG)
#define CCA		(RFSTATUS & CCA_FLAG)

// Various radio settings
#define PAN_COORDINATOR	0x10
#define ADR_DECODE	0x08
#define AUTO_CRC	0x20
#define AUTO_ACK	0x10
#define AUTO_TX2RX_OFF	0x08
#define RX2RX_TIME_OFF	0x04
#define ACCEPT_ACKPKT	0x01

//-----------------------------------------------------------------------------
// Command Strobe Processor (CSP) instructions
//-----------------------------------------------------------------------------
#define DECZ        do{RFST = 0xBF;                       }while(0)
#define DECY        do{RFST = 0xBE;                       }while(0)
#define INCY        do{RFST = 0xBD;                       }while(0)
#define INCMAXY(m)  do{RFST = (0xB8 | m);                 }while(0) // m < 8 !!
#define RANDXY      do{RFST = 0xBC;                       }while(0)
#define INT         do{RFST = 0xB9;                       }while(0)
#define WAITX       do{RFST = 0xBB;                       }while(0)
#define WAIT(w)     do{RFST = (0x80 | w);                 }while(0) // w < 64 !!
#define WEVENT      do{RFST = 0xB8;                       }while(0)
#define LABEL       do{RFST = 0xBA;                       }while(0)
#define RPT(n,c)    do{RFST = (0xA0 | (n << 3) | c);      }while(0) // n = TRUE/FALSE && (c < 8)
#define SKIP(s,n,c) do{RFST = ((s << 4) | (n << 3) | c);  }while(0) // && (s < 8)
#define STOP        do{RFST = 0xDF;                       }while(0)
#define SNOP        do{RFST = 0xC0;                       }while(0)
#define STXCALN     do{RFST = 0xC1;                       }while(0)
#define SRXON       do{RFST = 0xC2;                       }while(0)
#define STXON       do{RFST = 0xC3;                       }while(0)
#define STXONCCA    do{RFST = 0xC4;                       }while(0)
#define SRFOFF      do{RFST = 0xC5;                       }while(0)
#define SFLUSHRX    do{RFST = 0xC6;                       }while(0)
#define SFLUSHTX    do{RFST = 0xC7;                       }while(0)
#define SACK        do{RFST = 0xC8;                       }while(0)
#define SACKPEND    do{RFST = 0xC9;                       }while(0)
#define ISSTOP      do{RFST = 0xFF;                       }while(0)
#define ISSTART     do{RFST = 0xFE;                       }while(0)
#define ISTXCALN    do{RFST = 0xE1;                       }while(0)
#define ISRXON      do{RFST = 0xE2;                       }while(0)
#define ISTXON      do{RFST = 0xE3;                       }while(0)
#define ISTXONCCA   do{RFST = 0xE4;                       }while(0)
#define ISRFOFF     do{RFST = 0xE5;                       }while(0)
#define ISFLUSHRX   do{RFST = 0xE6;                       }while(0)
#define ISFLUSHTX   do{RFST = 0xE7;                       }while(0)
#define ISACK       do{RFST = 0xE8;                       }while(0)
#define ISACKPEND   do{RFST = 0xE9;                       }while(0)

#define PACKET_FOOTER_SIZE 2    //bytes after the payload

#endif

