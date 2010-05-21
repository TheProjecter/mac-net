/****************************************************************
 * file:	mac.c
 * data:	2010-05-21
 * description: Implement the mechanism required by MAC
 ***************************************************************/

#include "phy.h"
#include "mac.h"

/************************************************
 **	Define external variables	**
 ***********************************************/
struct queue mac_send_queue;	// Mac send data queue
MHR mac_header;			// Current MHR


/************************************************
 	**	Local variables		**
 ***********************************************/
static BOOL new_mhr_filled = FALSE;	// If new frame is filled to send
static MAC_PIB mac_pib;			// MAC PIB instant
static MAC_SERVICE a_mac_service;	// MAC Service instant
static MAC_STATE_ENUM mac_state;	// MAC state: Control FSM


/************************************************
 	**	Local Functions		**
 ***********************************************/
static void load_queue_data();		// Load the queue's data to send
static void mac_init_mhr();		// Initialize the mhr to the pre-defined type.
static void mac_fill_mhr(struct data_entry *payload);	// Fill in the MHR for current sending data
static void mac_FSM_tx_mode();		// Change the MAC FSM's state to tx data mode
static void mac_tx_data();		// Actually tx data to phy layer.
static void mac_tx_handler();		// Handle tx result or waiting for tx
static BOOL mac_check_new_frame();	// Check if new frame available
static void mac_analyse_new_frame();	// Analyse new frame


//does not turn on radio.
void mac_init()
{		
	// Initialize send queue
	queue_init(&mac_send_queue);
	// Initialize PHY layer
	phy_init();

	mac_state = MAC_STATE_IDLE;
        mac_set_tx_idle();

	// PIB
	mac_pib.macPANID = RADIO_DEFAULT_PANID;
	mac_pib.macSAddr = SHORTADDRL;
        mac_pib.macSAddr += (SHORTADDRH << 8);

	// Initialize mhr
	mac_init_mhr();
}

/************************************************
 * Description: The mac layer service FSM.
 *
 * Arguments:
 * 	None.
 * Return:
 * 	None.
 *
 * Date: 2010-05-20
 ***********************************************/
void mac_FSM()
{
	phy_FSM();

	// Check new data from radio
	if ( mac_check_new_frame() )
		mac_analyse_new_frame();

	// If Tx FSM is busy we need to call it
	if (mac_tx_busy())
		mac_tx_handler();
	
	// If the mac layer idle
	if (mac_tx_idle() && new_mhr_filled == FALSE)
		load_queue_data();

	//check background tasks here
	switch (mac_state)
	{
		case MAC_STATE_IDLE:
			break;
		case MAC_STATE_COMMAND_START:
			switch(a_mac_service.cmd)
			{
				case LRWPAN_SVC_MAC_ERROR:
					//dummy service, just return the status that was passed in
					mac_state = MAC_STATE_IDLE;
					break;
				
				case LRWPAN_SVC_MAC_GENERIC_TX:
					//send a generic packet with arguments specified by upper level
					mac_tx_data();
					mac_state = MAC_STATE_GENERIC_TX_WAIT;
					break;

				case LRWPAN_SVC_MAC_RETRANSMIT:
					//retransmit the last packet used for frames that are
					//only transmitted once because of no ACK request
					//assumes the TX lock is grabbed, and the TX buffer formatted.
					break;

				default:
					break;
			} //end switch cmd
			break;

		case MAC_STATE_GENERIC_TX_WAIT:
			// Just return;
			break;
		//this is used by MAC CMDs in general which send a packet with no ACK.
		case MAC_STATE_GENERIC_TX_WAIT_AND_UNLOCK:
			if (!mac_tx_idle())
				break;
			mac_state = MAC_STATE_IDLE;
			break;

		default:
			break;
	}// end for switch(mac_state)
}

/****************************************************************
 * Description: The TxInProgress bit is set when the mac_tx_data
 * 	function is called the first time to actually send the
 * 	packet. After that, each loop through the mac_tx_handler
 * 	checks to see if the TX started and finished correctly.
 * 	The FSM becomes idle if:
 * 	a. the PHY TX returns an error
 * 	b. the PHY TX returned success
 * 	c. the PHY TX returned failed and we exhausted retries.
 *
 * Arguments:
 * 	None.
 * Return:
 * 	None.
 *
 * Date: 2010-05-20
****************************************************************/
static void mac_tx_handler()
{
	// Ensure tx is in progress
	if(mac_tx_busy())
	{
		// PHY layer has finish the tx
		if (phy_idle())
		{
			// Copy the tx result
			a_mac_service.status = phy_tx_result();

#ifdef STATISTIC
			// If send success?
			if (a_mac_service.status == LRWPAN_STATUS_SUCCESS)
				statistic_mac_tx ++;
			else
				statistic_mac_err++;
#endif

			// Set mac tx to idle
			mac_set_tx_idle();
			mac_state = MAC_STATE_IDLE;

			// Change the mhr filled state, so we can
			// send data in the queue
			new_mhr_filled = FALSE;
			// Remove the data from queue
			queue_remove_next(&mac_send_queue);
		}
	}
}

/************************************************
 * Description: Pass the data to phy layer, assume
 * 	header format is preprocessed by mac_fill_mhr()
 *
 * Arguments:
 * 	None.
 * Return:
 * 	None.
 *
 * Date: 2010-05-20
 ***********************************************/
static void mac_tx_data()
{
	// If mac idle?
	if (mac_tx_idle())
	{
		// Start tx, set busy flags.
		mac_set_tx_busy();
	}

	// Hand to phy layer
	phy_tx_data();
}

/************************************************
 * Description: Get current PAN's ID.
 *
 * Arguments:
 * 	None.
 * Return:
 * 	UINT16 -- PAN ID
 *
 * Date: 2010-05-20
 ***********************************************/
UINT16 mac_get_PANID()
{
	return mac_pib.macPANID;
}

/************************************************
 * Description: Get this node's short address
 *
 * Arguments:
 * 	None.
 * Return:
 * 	UINT16 -- Node's short address
 *
 * Date: 2010-05-20
 ***********************************************/
UINT16 mac_get_addr()
{
	return mac_pib.macSAddr;
}

/************************************************
 * Desciption: If new frame is available.
 *
 * Arguments:
 * 	None.
 * Return:
 * 	TRUE -- If buffer has new frame.
 * 	FALSE -- If not
 *
 * Date: 2010-05-21
 ***********************************************/
static BOOL mac_check_new_frame()
{
	return !radio_buffer_empty();
}

/************************************************
 * Description: Analyse the new frame.
 *
 * Arguments:
 * 	None.
 * Return:
 * 	None.
 *
 * Date: 2010-05-21
 ***********************************************/
void mac_analyse_new_frame()
{
       UINT8 frm_len;	// Frame length: MHR(9), RSSI(1), CRC(1)

       frm_len = radio_get_frm_length();// Get the first frame's length
       /*****************************************
	* Handle the data to user
	****************************************/
       radio_drop_bytes(frm_len+1);	// Drop current frame
}

/************************************************
 * Description: Get the last packet's rssi value.
 *
 * Arguments:
 * 	None.
 * Return:
 * 	UINT8 -- Last packet's rssi value.
 *
 * Date: 2010-05-18
 ***********************************************/
UINT8 mac_get_rssi()
{
	return mac_pib.rx_rssi;
}

/************************************************
 * Description:
 *
 * Arguments:
 * Return:
 *
 * Date: 2010-05-18
 ************************************************/
static void load_queue_data()
{
	struct data_entry * queue_data;

         if (queue_is_empty(&mac_send_queue))
		 return;

	 // Get next data to send.
	 queue_data = queue_get_next(&mac_send_queue);

	 // Fill the MAC header.
	 mac_fill_mhr(queue_data);
	 // Ok, data available, try to send.
	 // Change to tx mode
	 mac_FSM_tx_mode();
 }

/************************************************
 * Description: Initialize the MHR to pre-defined type.
 * 	Please refer to CC2430 Data sheet: page(171)
 * 	for a detail information. Pre-defined:
 * 	1. Intra-PAN
 * 	2. Short address mode
 * 	3. Broadcast
 * 	4. No ack
 *
 * Arguments:
 * 	None.
 * Return:
 * 	None.
 *
 * Date: 2010-05-20
 ***********************************************/
static void mac_init_mhr()
{
	mac_header.fcflsb = LRWPAN_FRAME_TYPE_DATA|LRWPAN_FCF_INTRAPAN_MASK;
	mac_header.fcfmsb = LRWPAN_FCF_DSTMODE_SADDR|LRWPAN_FCF_SRCMODE_SADDR;
	// DSN will filled when loading MHR
	
	mac_header.dest_PANID = mac_get_PANID();
	mac_header.dest_addr = 0xffff;	// Broadcast
	mac_header.src_addr = mac_get_addr();
	mac_header.payload = NULL;
}

/************************************************
 * Description: Fill the MHR's dsn and mac_payload field.
 *
 * Arguments:
 * 	payload -- The MAC payload data.
 * Return:
 * 	None.
 *
 * Date: 2010-05-20
 ***********************************************/
static void mac_fill_mhr(struct data_entry *payload)
{
	mac_header.dsn = hal_get_random_byte();
	mac_header.payload = payload;

	// Change the mhr filled state
	new_mhr_filled = TRUE;
}
/************************************************
 * Description: Change the FSM to tx data.
 *
 * Arguments:
 * 	None.
 * Return:
 * 	None.
 *
 * Date: 2010-05-20
 ***********************************************/
static void mac_FSM_tx_mode()
{
	mac_state = MAC_STATE_COMMAND_START;
	a_mac_service.cmd = LRWPAN_SVC_MAC_GENERIC_TX;
}

