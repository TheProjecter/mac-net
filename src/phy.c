/****************************************************************
 * file:	phy.c
 * data:	2009-03-18
 * description: Implement the services required by up layer.
 ***************************************************************/

#include "phy.h"

PHY_PIB phy_pib;
PHY_SERVICE a_phy_service;
PHY_STATE_ENUM phy_state;

//static tmp space for that is used by NET, APS, MAC layers
//since only one TX can be in progress at a time, there will be
//not contention for this.
//The current frame is built up in this space, in reverse transmit order.
UINT8 tmpTxBuff[LRWPAN_MAX_FRAME_SIZE];

void phy_init(void )
{
	phy_state = PHY_STATE_IDLE;
}

void phy_FSM(void)
{
	//check background tasks here
	switch (phy_state)
	{
		case PHY_STATE_IDLE:
			hal_idle();	//Hal Layer might want to do something in idle state
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
					a_phy_service.status =
						radio_send_packet(phy_pib.currentTxFlen, phy_pib.currentTxFrm);
					if (a_phy_service.status == LRWPAN_STATUS_SUCCESS)
					{
						//TX started, wait for it to end.
						phy_state = PHY_STATE_TX_WAIT;

						// debug use
						// LED_GREEN_TOGGLE();
					}
					else
					{

						// something failed, will give up on this,
						// MAC can take action if it wants
						phy_state = PHY_STATE_IDLE;
					}
					break;
				default: break;
			}//end switch cmd
			break;
		case PHY_STATE_TX_WAIT:  //wait for TX out of radio to complete or timeout
			if (phy_pib.flags.bits.txFinished)
			{

				phy_state = PHY_STATE_IDLE;
			}
			else if  (MAC_TIMER_NOW_DELTA(phy_pib.txStartTime) > MAX_TX_TRANSMIT_TIME)
			{

				//should not happen, indicate an error to console
				a_phy_service.status = LRWPAN_STATUS_PHY_TX_FINISH_FAILED;
				//no action for now, will see if this happens
				phy_state = PHY_STATE_IDLE;
			}
			break;
		default:
			break;
	}//end switch phy_state
}

//call back from HAL to here, can be empty functions
//not needed in this stack
void phyRxCallback(void)
{
}

void phy_tx_start_callback(void)
{
	phy_pib.txStartTime = mac_timer_get();
}

void phy_tx_end_callback(void)
{
	phy_pib.flags.bits.txFinished = 1;   //TX is finished.
}
