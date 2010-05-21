/****************************************************************
 * file:	mac.h
 * data:	2010-05-21
 * description:	The MAC layer, define the parameters.
 ***************************************************************/

#ifndef MAC_H
#define MAC_H

#include "queue.h"

// Mac layer's PAN Information Base
typedef struct _MAC_PIB {
	union _MAC_PIB_flags {
		struct {
			unsigned ackPending:1;		// Waiting for ACK
			unsigned TxInProgress:1;	//MAC TX FSM state
		}bits;
	}flags;
	UINT16 macPANID;	// PAN ID
	UINT16 macSAddr;	// My short address

        UINT8 rx_rssi;
}MAC_PIB;

// Mac FSM state enumeration
typedef enum _MAC_STATE_ENUM {
	MAC_STATE_IDLE,
	MAC_STATE_COMMAND_START,
	MAC_STATE_GENERIC_TX_WAIT,
	MAC_STATE_GENERIC_TX_WAIT_AND_UNLOCK,
	MAC_STATE_HANDLE_ORPHAN_NOTIFY,
	MAC_STATE_ORPHAN_WAIT1,
	MAC_STATE_ORPHAN_WAIT2,
	MAC_STATE_ASSOC_REQ_WAIT1,
	MAC_STATE_ASSOC_REQ_WAIT2,
	MAC_STATE_SEND_BEACON_RESPONSE,
	MAC_STATE_SEND_ASSOC_RESPONSE
}MAC_STATE_ENUM;

typedef struct _MAC_SERVICE {
	LRWPAN_SVC_ENUM cmd;
	LRWPAN_STATUS_ENUM status;
}MAC_SERVICE;


/************************************************
 * Structure: MAC Header(MHR), bytes as follows. CC2430 Page(170)
 * Bytes	2			1		6			n
 * 	|  Frame Control Field	|Data Sequence Number|Address Information| MAC Payload|
 *
 * Date: 2010-05-20
 ***********************************************/
typedef struct _MHR {
	UINT8 fcflsb;	// FCF least significant byte
	UINT8 fcfmsb;	// FCF most significant byte
	UINT8 dsn;	// Data Sequence Number

	UINT16 dest_PANID;
	UINT16 dest_addr;
	UINT16 src_addr;

	struct data_entry *payload;
}MHR;


/************************************************
 	**	External Variables	**
 ***********************************************/
#define MHR_LENGTH	9	// Mac Header length(not include payload)
extern MHR mac_header;		// Mac Header
extern struct queue mac_send_queue;	// Mac tx queue


/************************************************
 	**	External Functions	**
 ***********************************************/
extern void mac_init();
extern void mac_FSM();
extern UINT16 mac_get_addr();
extern UINT16 mac_get_PANID();
extern UINT8 mac_get_rssi();	// Get the rssi value


/************************************************
 	**	Constant Variable	**
 ***********************************************/
#define MAC_DATA_RSSI_CRC 2	// RSSI and CRC byte lenghth in rx packet


#define mac_idle() (mac_state == MAC_STATE_IDLE)
#define mac_busy() (mac_state != MAC_STATE_IDLE)

#define mac_tx_idle() (!mac_pib.flags.bits.TxInProgress)
#define mac_tx_busy() (mac_pib.flags.bits.TxInProgress)
#define mac_set_tx_busy() mac_pib.flags.bits.TxInProgress = 1
#define mac_set_tx_idle() mac_pib.flags.bits.TxInProgress = 0

#define mac_do_service() \
	a_mac_service.status = LRWPAN_STATUS_MAC_INPROGRESS;\
	mac_state = MAC_STATE_COMMAND_START;\
	mac_FSM();\

#endif
