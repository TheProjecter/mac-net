/****************************************************************
 * file:	phy.h
 * data:	2010-05-21
 * description:	The physcial layer
 ***************************************************************/

#ifndef PHY_H
#define PHY_H

#include "hal.h"

// PHY layer's PAN Information Base
typedef struct _PHY_PIB {
	union _PHY_DATA_flags {
		struct {
			unsigned txFinished:1;	//indicates if TX at PHY level is finished...
		}bits;
	}flags;
}PHY_PIB;

// PHY FSM state enumeration
typedef enum _PHY_STATE_ENUM {
	PHY_STATE_IDLE,
	PHY_STATE_COMMAND_START,
	PHY_STATE_TX_WAIT
}PHY_STATE_ENUM;

typedef struct _PHY_SERVICE {
	LRWPAN_SVC_ENUM cmd;
	LRWPAN_STATUS_ENUM status;
}PHY_SERVICE;


/************************************************
 	**	external Variables	**
 ***********************************************/

/************************************************
 	**	External Funcions	**
 ***********************************************/
extern void phy_tx_data();
extern void phy_FSM();
extern void phy_init();
extern void phy_tx_end_callback();
extern LRWPAN_STATUS_ENUM phy_tx_result();
extern BOOL phy_idle();


//cannot overlap services, make this a macro to reduce stack depth
#define phy_do_service() \
  a_phy_service.status = LRWPAN_STATUS_PHY_INPROGRESS;\
  phy_state = PHY_STATE_COMMAND_START;\
  phy_FSM();

#endif

