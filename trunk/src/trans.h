/************************************************
 * File: present.h
 *
 * Description: transport layer staff, 
 * 		sending and receiving with formatted frames
 *
 * Date: 2009-10-27
 ************************************************/
#ifndef TRANS_H
#define TRANS_H

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

/************************************************
 	**	External Functions	**
 ***********************************************/
extern void trans_init();
extern BOOL trans_frm_avail();
extern void trans_frm_parse();

#endif
