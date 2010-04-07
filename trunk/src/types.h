/****************************************************************
 * file:	types.h
 * date:	2009-03-12
 * description:	define types
 ***************************************************************/
#ifndef	TYPES_H
#define	TYPES_H

typedef unsigned char const __code ROMCHAR ;
typedef	unsigned char		UINT8;
typedef	signed char		INT8;
typedef	unsigned short		UINT16;
typedef	signed short		INT16;
typedef	unsigned long		UINT32;
typedef	signed long		INT32;
typedef unsigned char		BOOL;
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0
#endif

typedef enum _LRWPAN_STATUSENUM {
	LRWPAN_STATUS_SUCCESS = 0,
	LRWPAN_STATUS_PHY_FAILED,
	LRWPAN_STATUS_PHY_INPROGRESS,		//still working for splitphase operations
	LRWPAN_STATUS_PHY_RADIO_INIT_FAILED,
	LRWPAN_STATUS_PHY_TX_PKT_TOO_BIG,
	LRWPAN_STATUS_PHY_TX_START_FAILED,
	LRWPAN_STATUS_PHY_TX_FINISH_FAILED,
	LRWPAN_STATUS_PHY_CHANNEL_BUSY,
	LRWPAN_STATUS_MAC_FAILED,
	LRWPAN_STATUS_MAC_MAX_RETRIES_EXCEEDED,	//exceeded max retries
	LRWPAN_STATUS_MAC_INPROGRESS,		//still working for splitphase operations
	LRWPAN_STATUS_MAC_TX_FAILED,		//MAC Tx Failed, retry count exceeded
        LRWPAN_STATUS_RT_INPROGRESS,
	USER_ENQUE_SUCCESS,
	USERLOOPBACK,
	USERINVALIDADRESS,
	USERSENDBUFFERFULL,
	USERSTACKBUSY,
}LRWPAN_STATUS_ENUM;

typedef enum _LRWPAN_SVC_ENUM {
	LRWPAN_SVC_NONE,
	LRWPAN_SVC_PHY_INIT_HAL,
	LRWPAN_SVC_PHY_TX_DATA,
	LRWPAN_SVC_MAC_GENERIC_TX,
	LRWPAN_SVC_MAC_RETRANSMIT,
	LRWPAN_SVC_MAC_ERROR,
	LRWPAN_SVC_RT_GENERIC_TX,
	LRWPAN_SVC_RT_ERROR,
} LRWPAN_SVC_ENUM;

#endif

