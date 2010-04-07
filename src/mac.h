/****************************************************************
 * file:	mac.h
 * data:	2009-03-19
 * description:	The MAC layer, define the parameters.
 ***************************************************************/

#ifndef MAC_H
#define MAC_H

#include "phy.h"

#define aMaxBE 5
#define aMinBE 0
#define aUnitBackoffPeriod 20		//in symbols
#define macMaxCSMABackoffs 4
#define MAC_DATA_RSSI_CRC 2             // RSSI and CRC byte lenghth in rx packet

#define aMaxFrameRetries 2

//default timeout on network responses
#define MAC_GENERIC_WAIT_TIME      MSECS_TO_MACTICKS(20)
#define MAC_ASSOC_WAIT_TIME        MAC_GENERIC_WAIT_TIME
#define MAC_ORPHAN_WAIT_TIME       MAC_GENERIC_WAIT_TIME

// Short address
typedef UINT16 SADDR;

typedef struct _MAC_PIB {
	UINT32 macAckWaitDuration;
	union _MAC_PIB_flags {
		UINT32 val;
		struct {
			unsigned ackPending:1;
			unsigned TxInProgress:1;	//MAC TX FSM state
			unsigned macPending:1;		//mac CMD pending in the RX buffer
		}bits;
	}flags;
	UINT16 macPANID;
	UINT8 macDSN;
	UINT16 macSAddr;
	UINT8 macMaxAckRetries;
	struct {
		unsigned maxMaxCSMABackoffs:3;
		unsigned macMinBE:2;
	}misc;
	UINT32 tx_start_time;		//time that packet was sent
	UINT32 last_data_rx_time;	//time that last data rx packet was received that was accepted by this node

	UINT8 rxTail;			//tail pointer of mac_rx_frames
	UINT8 rxHead;			//head pointer of mac_rx_frames
volatile        BOOL rxFull;                    // Rx buffer is full if TRUE
        UINT8 rx_rssi;
}MAC_PIB;

//used for parsing of RX data
typedef struct _MAC_RX_DATA {
	UINT8 *orgpkt;			//original packet
	UINT8 fcflsb;
	UINT8 fcfmsb;
        UINT16 DestPANID;
        UINT16 SrcPANID;
        SADDR DestAddr;			//dst short address
	SADDR SrcAddr;			//src short address
	UINT8 pload_offset;		//start of payload
}MAC_RX_DATA;

typedef struct _MAX_TX_DATA {
	SADDR DestAddr;
        UINT16 SrcPANID;
        UINT16 DestPANID;
	SADDR SrcAddr;         //src address, either short or long, this holds short address version
	                       //if long is needed, then get this from HAL layer
	UINT8 fcflsb;          //frame control bits specify header bits
	UINT8 fcfmsb;
}MAC_TX_DATA;

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

extern MAC_PIB mac_pib;
extern MAC_SERVICE a_mac_service;
extern MAC_STATE_ENUM mac_state;
extern MAC_TX_DATA a_mac_tx_data;
extern MAC_RX_DATA a_mac_rx_data;
extern UINT8 mac_rx_frames[LRWPAN_RXBUFF_SIZE][LRWPAN_MAX_FRAME_SIZE];

void mac_init(void);
void mac_FSM(void);
UINT16 mac_get_addr(void);
UINT16 mac_get_PANID(void);
LRWPAN_STATUS_ENUM mac_init_radio(void);

BOOL mac_rx_buff_empty(void);
BOOL mac_rx_buff_full(void);
UINT8 *mac_get_rx_packet(void);
void mac_free_rx_packet();
void macRxCallback(UINT8 *ptr, UINT8 rssi);
void macTxCallback(void);


BOOL mac_do_send(UINT16 dst_addr, UINT8 *ptr, UINT8 len);


//SADDR macGetShortAddr();
#define macGetShortAddr()   (mac_addr_tbl[0].saddr)

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
