/*
 * file: trans.h
 * date: 2009-10-27
 * brief: transaction layer staff,
 * 		sending and receiving with formatted frames
 */
#ifndef TRANS_H
#define TRANS_H

// #include "dsr_route.h"		// to use user_send_data function

#define FRAME_MAX_LEN	127

#define FRAME_DELIMETER 0X7E
#define FRAME_TYPE_TX_DATA	0x10	// send data from pc/arm to cc2430
#define FRAME_TYPE_RX_DATA	0x90	// send data from 2430 to pc/arm
#define FRAME_TYPE_SET_PARA	0x08	// set parameters from pc/arm
#define FRAME_TYPE_GET_PARA	0x09	//  pc/arm is querying for para value
#define FRAME_TYPE_RET_PARA	0x8B	// return parameters to pc/arm
#define FRAME_TYPE_GET_STATISTIC 0x70	// pc/arm to cc2430
#define FRAME_TYPE_RET_STATISTIC 0x71	//  cc2430 return STATISTIC info to pc/arm
#define FRAME_TYPE_CLR_STATISTIC 0X72	// reset the statistic info


#define FRAME_TYPE_GET_DEBUG	0x75	//   pc/arm to cc2430, TODO: not implemented
#define FRAME_TYPE_RET_DEBUG	0x76	// return DEBUG info to pc/arm, TODO: not implemented


#define PARA_TYPE_TXPOWER	0x01
#define PARA_TYPE_MYSADDR	0x02	// my short addr, 2 bytes
#define PARA_TYPE_RSSI		0x03	// received signal strength indicator
#define PARA_TYPE_TEMPERATURE	0x04	// temperature
#define PARA_TYPE_VERSION	0X10	// firmware version	TODO: not implemented
#define PARA_TYPE_BAUDRATE	0x20



void trans_init();
BOOL trans_frm_avail();
void trans_frm_parse();

void trans_fram_assemble(UINT16 src_addr, UINT8 *ptr, UINT8 len);		// called when receive packet from other node

void trans_put_statistic();	// return statistic info to PC/ARM

void trans_fetch_from_mac();	// this is for temp use!!!

void	trans_clear_statistic();
void trans_set_baudrate(UINT8 val);

#endif
