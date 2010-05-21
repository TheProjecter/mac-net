/****************************************************************
 * file:	phy.c
 * data:	2010-05-21
 * description: Implement the services required by up layer.
 ***************************************************************/

#include "phy.h"

/************************************************
 	**	Local variables		**
 ***********************************************/
static PHY_PIB phy_pib;
static PHY_SERVICE a_phy_service;
static PHY_STATE_ENUM phy_state;


/************************************************
 * Description: Initialize PHY layer to idle.
 *
 * Arguments:
 * 	None.
 * Return:
 * 	None.
 *
 * Date: 2010-05-20
 ***********************************************/
void phy_init()
{
	phy_state = PHY_STATE_IDLE;	// PHY layer idle.
}

/************************************************
 * Description: PHY layer's main FSM service.
 *
 * Arguments:
 * 	None.
 * Return:
 * 	None.
 *
 * Date: 2010-05-20
 ***********************************************/
void phy_FSM()
{
	switch (phy_state)
	{
		case PHY_STATE_IDLE:
			hal_idle();	// Hal Layer might want to do something in idle state
			break;
		case PHY_STATE_COMMAND_START:
			switch(a_phy_service.cmd)
			{
				case LRWPAN_SVC_PHY_INIT_HAL:	//not split phase
					a_phy_service.status = hal_init();
					phy_state = PHY_STATE_IDLE;
					break;
				case LRWPAN_SVC_PHY_TX_DATA:
					phy_pib.flags.bits.txFinished = 0;
					a_phy_service.status = radio_send_packet();
					if (a_phy_service.status == LRWPAN_STATUS_SUCCESS)
					{
						//TX started, wait for it to end.
						phy_state = PHY_STATE_TX_WAIT;
					}
					else
					{
						// Something failed, will give up on this,
						// MAC can take action if it wants
						phy_state = PHY_STATE_IDLE;
					}
					break;
				default: break;
			}//end switch cmd
			break;
		case PHY_STATE_TX_WAIT:  //wait for TX out of radio to complete or timeout
			if (phy_pib.flags.bits.txFinished)
				phy_state = PHY_STATE_IDLE;
			break;
		default:
			break;
	}//end switch phy_state
}

/************************************************
 * Description: Set flag indicates current tx finished.
 *
 * Arguments:
 * 	None.
 * Return:
 * 	None.
 *
 * Date: 2010-05-21
 ***********************************************/
void phy_tx_end_callback()
{
	phy_pib.flags.bits.txFinished = 1;	// TX is finished.
}

/************************************************
 * Description: Enter tx data service.
 *
 * Arguments:
 * 	None.
 * Return:
 * 	None.
 *
 * Date: 2010-05-20
 ***********************************************/
void phy_tx_data()
{
	// Tx data service.
	a_phy_service.cmd = LRWPAN_SVC_PHY_TX_DATA;
	phy_do_service();
}

/************************************************
 * Description: Return the tx result of current packet
 *
 * Arguments:
 * 	None.
 * Return:
 * 	LRWPAN_STATUS_ENUM -- Tx status.
 *
 * Date: 2010-05-21
 ***********************************************/
LRWPAN_STATUS_ENUM phy_tx_result()
{
	return a_phy_service.status;
}

/************************************************
 * Description: If phy layer is idle
 *
 * Arguments:
 * 	None.
 * Return:
 * 	TRUE -- If phy in idle state
 * 	FALSE -- If tx in progress
 *
 * Date: 2010-05-21
 ***********************************************/
BOOL phy_idle()
{
	return (phy_state == PHY_STATE_IDLE);
}

