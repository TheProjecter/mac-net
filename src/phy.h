/****************************************************************
 * file:	phy.h
 * data:	2009-03/18
 * description:	The physcial layer
 ***************************************************************/

#ifndef PHY_H
#define PHY_H

#include "hal.h"

#define aMaxPHYPacketSize 127
#define aTurnaroundTime 12

#define MAX_TX_TRANSMIT_TIME (SYMBOLS_TO_MACTICKS(300))  //a little long..

typedef enum _RADIO_STATUS_ENUM {
	RADIO_STATUS_OFF,
	RADIO_STATUS_RX_ON,
	RADIO_STATUS_TX_ON,
	RADIO_STATUS_RXTX_ON
}RADIO_STATUS_ENUM;

typedef struct _PHY_PIB {
	UINT8 phyTransmitPower;
	UINT8 phyCCAMode;
	union _PHY_DATA_flags {
		UINT8 val;
		struct {
			unsigned txFinished:1;	//indicates if TX at PHY level is finished...
			unsigned txBuffLock:1;	//lock the TX buffer.
		}bits;
	}flags;
	UINT32 txStartTime;
	UINT8 *currentTxFrm;	//current frame
	UINT8 currentTxFlen;	//current TX frame length
}PHY_PIB;

//used for radio initialization
typedef union _RADIO_FLAGS {
	UINT8 val;
	struct _RADIO_FLAGS_bits {
		unsigned listen_mode:1;		// if true, then put radio in listen mode,
						// which is non-auto ack, no address decoding
		}bits;
}RADIO_FLAGS;

typedef union _PHY_ARGS {
	struct _PHY_INIT_RADIO {
		RADIO_FLAGS radio_flags;
	}phy_init_radio_args;
}PHY_ARGS;

typedef struct _PHY_SERVICE {
	LRWPAN_SVC_ENUM cmd;
	PHY_ARGS args;
	LRWPAN_STATUS_ENUM status;
}PHY_SERVICE;

typedef enum _PHY_STATE_ENUM {
	PHY_STATE_IDLE,
	PHY_STATE_COMMAND_START,
	PHY_STATE_TX_WAIT
} PHY_STATE_ENUM;

extern PHY_STATE_ENUM phy_state;
extern PHY_PIB phy_pib;
extern PHY_SERVICE a_phy_service;
extern UINT8 tmpTxBuff[LRWPAN_MAX_FRAME_SIZE];

//prototypes
void phy_FSM(void);
void phy_init(void );
void phy_tx_end_callback(void);

#define phy_idle() (phy_state == PHY_STATE_IDLE)
#define phy_busy() (phy_state != PHY_STATE_IDLE)

#define phy_tx_locked()   (phy_pib.flags.bits.txBuffLock == 1)
#define phy_tx_unLocked()   (phy_pib.flags.bits.txBuffLock == 0)

#define phy_grab_tx_lock()	phy_pib.flags.bits.txBuffLock = 1
#define phy_release_tx_lock() phy_pib.flags.bits.txBuffLock = 0

//cannot overlap services, make this a macro to reduce stack depth
#define phy_do_service() \
  a_phy_service.status = LRWPAN_STATUS_PHY_INPROGRESS;\
  phy_state = PHY_STATE_COMMAND_START;\
  phy_FSM();

#endif

