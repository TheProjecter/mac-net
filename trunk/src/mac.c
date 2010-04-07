/****************************************************************
 * file:	mac.c
 * data:	2009-03-19
 * description: Implement the mechanism required by MAC
 ***************************************************************/

#include "mac.h"
// #include "dsr_header.h"

typedef enum _MAC_RXSTATE_ENUM {
	MAC_RXSTATE_IDLE,
	MAC_RXSTATE_NWK_HANDOFF,
	MAC_RXSTATE_CMD_PENDING
}MAC_RXSTATE_ENUM;

//static MAC_RXSTATE_ENUM mac_rx_state;
MAC_PIB mac_pib;
MAC_SERVICE a_mac_service;
MAC_STATE_ENUM mac_state;
// A circulate array, data added indicates by mac_pib.rxHead,
// data removed indicates by mac_pib.rxTail
UINT8 mac_rx_frames[LRWPAN_RXBUFF_SIZE][LRWPAN_MAX_FRAME_SIZE];

//there can only be one TX in progress at a time, so
//a_mac_tx_data contains the arguments for that TX.
MAC_TX_DATA a_mac_tx_data;

//this is used for parsing of current packet.
MAC_RX_DATA a_mac_rx_data;

LRWPAN_STATUS_ENUM macTxFSM_status;

//local functions
static void mac_tx_data(void);
static void mac_tx_FSM(void);

//does not turn on radio.
void mac_init(void)
{
	phy_init();

	mac_state = MAC_STATE_IDLE;
	//mac_rx_state = MAC_RXSTATE_IDLE;
        mac_set_tx_idle();
	mac_pib.flags.val = 0;
	mac_pib.rxTail = 0;
	mac_pib.rxHead = 0;
        mac_pib.rxFull = FALSE;
	mac_pib.macPANID = RADIO_DEFAULT_PANID;
        mac_pib.macAckWaitDuration = SYMBOLS_TO_MACTICKS(120);
	mac_pib.macSAddr = SHORTADDRL;
        mac_pib.macSAddr += (SHORTADDRH << 8);

	a_mac_tx_data.SrcPANID = mac_get_PANID();
	a_mac_tx_data.DestPANID = RADIO_DEFAULT_PANID;
		
}

void mac_FSM(void)
{
	phy_FSM();
	//if TxFSM is busy we need to call it
	if (mac_tx_busy())
		mac_tx_FSM();
	//mac_rx_FSM();

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
					//mac_set_tx_busy();
					macTxFSM_status = LRWPAN_STATUS_MAC_INPROGRESS;
					a_phy_service.cmd = LRWPAN_SVC_PHY_TX_DATA;
					phy_do_service();
					mac_state = MAC_STATE_GENERIC_TX_WAIT;
					break;

				default:
					break;
			} //end switch cmd
			break;

		case MAC_STATE_GENERIC_TX_WAIT:
			if (mac_tx_busy())
				break;
			//TX is finished, copy status
			//a_mac_service.status = macTxFSM_status;
			mac_state = MAC_STATE_IDLE;
			break;

		//this is used by MAC CMDs in general which send a packet with no ACK.
		case MAC_STATE_GENERIC_TX_WAIT_AND_UNLOCK:
			if (!mac_tx_idle())
				break;
			//TX is finished, copy status
			a_mac_service.status = macTxFSM_status;
			mac_state = MAC_STATE_IDLE;
			//also unlock TX buffer
			phy_release_tx_lock();
			break;

		default:
			break;
	}// end for switch(mac_state)
}

/****************************************************************
 * The TxInProgress bit is set when the mac_tx_data function
 * is called the first time to actually format the header and
 * send the packet. After that, each loop through the mac_tx_FSM
 * checks to see if the TX started and finished correctly, and
 * if an ACK was received if one was requested.  The FSM becomes
 * idle if:
 * a. the PHY TX returns an error
 * b. the PHY TX returned success and either no ACK was
 * 	requested or we received the correct ACK
 * c. the PHY TX returned success and we exhausted retries.
****************************************************************/
static void mac_tx_FSM(void)
{
	if(mac_tx_busy())
	{
		//we are not idle
		if (phy_idle())
		{
			//cannot check anything until PHY is idle
			if (a_phy_service.status != LRWPAN_STATUS_SUCCESS)
			{
				//don't bother waiting for ACK, TX did not start correctly
				if (mac_pib.macMaxAckRetries--)
				{
					//TX did not start correctly, but still try again even if PHY failed
					//mac_pib.flags.bits.ackPending = 0;
					mac_state = MAC_STATE_COMMAND_START;
					a_mac_service.cmd = LRWPAN_SVC_MAC_RETRANSMIT;
					//mac_tx_data();  //reuse the last packet.
				}
				else
				{

					a_mac_service.status = a_phy_service.status; //return status
					mac_pib.flags.bits.ackPending = 0;
					mac_set_tx_idle();  //mark TX as idle
					mac_state = MAC_STATE_IDLE;
				}
			}
			else
			{
				if(!mac_pib.flags.bits.ackPending)
				{
					//either no ACK requested or ACK has been received

					a_mac_service.status = LRWPAN_STATUS_SUCCESS;
					mac_state = MAC_STATE_IDLE;
					mac_set_tx_idle();  //finished successfully, mark as idle
				}
				// Tx success and ACK received but the ACK's timeout
				else if (MAC_TIMER_NOW_DELTA(mac_pib.tx_start_time)> mac_pib.macAckWaitDuration)
				{
					// ACK timeout
					if (mac_pib.macMaxAckRetries--)
					{
						//retry...
						mac_state = MAC_STATE_COMMAND_START;
						a_mac_service.cmd = LRWPAN_SVC_MAC_RETRANSMIT;
//						uart_putch('M');
					}
					else
					{
						//retries are zero. We have failed.

						a_mac_service.status = LRWPAN_STATUS_MAC_MAX_RETRIES_EXCEEDED;
						mac_set_tx_idle();
						mac_state = MAC_STATE_IDLE;
					}
				}
			}
		} // end for -- if (phy_idle())
	} // end for -- if(mac_tx_busy())
}

//Add the MAC header, then send it to PHY
static void mac_tx_data(void)
{
	UINT8 dstmode, srcmode;

//	 UINT8 i;

	mac_pib.flags.bits.ackPending = 0;
	if (mac_tx_idle())
	{
		//first time we are sending this packet, format the header
		//used static space for header. If need to store, will copy it later
		dstmode = LRWPAN_GET_DST_ADDR(a_mac_tx_data.fcfmsb);
		srcmode = LRWPAN_GET_SRC_ADDR(a_mac_tx_data.fcfmsb);

		//format src Address
		switch(srcmode)
		{
			case LRWPAN_ADDRMODE_SADDR:
				phy_pib.currentTxFrm--;
				*phy_pib.currentTxFrm = (UINT8)(a_mac_tx_data.SrcAddr >> 8);
				phy_pib.currentTxFrm--;
				*phy_pib.currentTxFrm = (UINT8)a_mac_tx_data.SrcAddr;
				phy_pib.currentTxFlen=phy_pib.currentTxFlen+2;
				break;
			default:
				break;
		}
		
		//format src PANID
		if( !LRWPAN_GET_INTRAPAN(a_mac_tx_data.fcflsb) &&
        srcmode != LRWPAN_ADDRMODE_NOADDR)
		{
			phy_pib.currentTxFrm--;
			*phy_pib.currentTxFrm = (UINT8)(a_mac_tx_data.SrcPANID >> 8);
			phy_pib.currentTxFrm--;
			*phy_pib.currentTxFrm = (UINT8)a_mac_tx_data.SrcPANID;
			phy_pib.currentTxFlen=phy_pib.currentTxFlen+2;
		}
		
		//format dst Address
		switch(dstmode)
		{
			case LRWPAN_ADDRMODE_SADDR:
				phy_pib.currentTxFrm--;
				*phy_pib.currentTxFrm = (UINT8)(a_mac_tx_data.DestAddr >> 8);
				phy_pib.currentTxFrm--;
				*phy_pib.currentTxFrm = (UINT8)a_mac_tx_data.DestAddr;
				phy_pib.currentTxFlen=phy_pib.currentTxFlen+2;
				break;
			default:
				break;
		}
		
                //format dst PANID, will be present if both dst is nonzero
		if (dstmode != LRWPAN_ADDRMODE_NOADDR)
		{
			phy_pib.currentTxFrm--;
			*phy_pib.currentTxFrm = (UINT8) (a_mac_tx_data.DestPANID >> 8);
			phy_pib.currentTxFrm--;
			*phy_pib.currentTxFrm = (UINT8)a_mac_tx_data.DestPANID;
			phy_pib.currentTxFlen=phy_pib.currentTxFlen+2;
		}
		
		//format dsn
		mac_pib.macDSN = hal_get_random_byte();
		phy_pib.currentTxFrm--;
		*phy_pib.currentTxFrm = mac_pib.macDSN; //set DSN
		
		//format MSB Fcontrol
		phy_pib.currentTxFrm--;
		*phy_pib.currentTxFrm = a_mac_tx_data.fcfmsb;
		
		//format LSB Fcontrol
		phy_pib.currentTxFrm--;
		*phy_pib.currentTxFrm = a_mac_tx_data.fcflsb;
		if (LRWPAN_GET_ACK_REQUEST(a_mac_tx_data.fcflsb))
		{
			mac_pib.flags.bits.ackPending = 1;  //we are requesting an ack for this packet
		}
		phy_pib.currentTxFlen = phy_pib.currentTxFlen + 3; //DSN, FCFLSB, FCFMSB

		
		//now send the data, ignore the GTS and INDIRECT bits for now
		mac_set_tx_busy();
		mac_pib.macMaxAckRetries = aMaxFrameRetries;
		macTxFSM_status = LRWPAN_STATUS_MAC_INPROGRESS;
	}

	
	/*
	// for debug
	uart_putstr("\rms: ");
	for (i = 0; i < phy_pib.currentTxFlen; i++) {
		uart_puthex(*(phy_pib.currentTxFrm+i));
		uart_putch(' ');
	}
	uart_putch('\r');
*/

	a_phy_service.cmd = LRWPAN_SVC_PHY_TX_DATA;
	phy_do_service();
}

UINT16 mac_get_PANID(void)
{
	return mac_pib.macPANID;
}

UINT16 mac_get_addr()
{
	return mac_pib.macSAddr;
}

BOOL mac_rx_buff_full(void)
{
	BOOL tmp;
	tmp = mac_pib.rxFull;
	return tmp;
}

BOOL mac_rx_buff_empty(void)
{
	return(!(mac_pib.rxFull) && mac_pib.rxTail == mac_pib.rxHead );
}

//this does NOT remove the packet from the buffer
UINT8 *mac_get_rx_packet()
{
	if (mac_rx_buff_empty())
		return(NULL);

	return(mac_rx_frames[mac_pib.rxTail]);
}

//frees the first packet in the buffer.
void mac_free_rx_packet()
{
	BOOL tmp;
	mac_pib.rxTail++;
   if (mac_pib.rxTail >=LRWPAN_RXBUFF_SIZE)
   	mac_pib.rxTail = 0;

   tmp = mac_pib.rxFull;
	if (tmp == TRUE)
	{
		mac_pib.rxFull = FALSE;
	}
}

//called by HAL when TX for current packet is finished
void macTxCallback(void)
{
	if ( mac_pib.flags.bits.ackPending == 1)
        {
		//we are requesting an ack for this packet
		//record the time of this packet
		mac_pib.tx_start_time = mac_timer_get();
	}
}

//we pass in the pointer to the packet first byte is packet length
void macRxCallback(UINT8 *ptr, UINT8 rssi)
{
	//if this is an ACK, update the current timeout, else indicate
	// that an ACK is pending
	// check length, check frame control field
	if ((*ptr == LRWPAN_ACKFRAME_LENGTH ) && LRWPAN_IS_ACK(*(ptr+1)))
	{
		//do not save ACK frames
		//this is an ACK, see if it is our ACK, check DSN
		if (*(ptr+3) == mac_pib.macDSN)
		{
			//DSN matches, assume this is our ack, clear the ackPending flag
			mac_pib.flags.bits.ackPending = 0;
		}
	}
	else
	{
		//save the packet, we assume the Physical/Hal layer has already checked
		//if the MAC buffer has room save it.
		mac_pib.rx_rssi = rssi;	//save RSSI value
	}
}

/**********************************************************
    ******************  Addtional functions  **********
 *********************************************************/
void usrIntCallback()
{
	/*
	UINT8 data_len = 0;
	UINT8 *ptr = NULL;

	if (!mac_rx_buff_empty())
	{
		//parse_packet(mac_rx_frames[mac_pib.rxTail]);
		ptr = mac_rx_frames[mac_pib.rxTail];
		data_len = *ptr - 11 - MAC_DATA_RSSI_CRC;
		nwk_get_critical_struct(ptr + 12, data_len);
	}*/
}

/*
 * this is the main interface for upper layer
 */
BOOL mac_do_send(UINT16 dst_addr, UINT8 *ptr, UINT8 len)
{
	UINT8 i;

	//grab the TX buffer lock before proceeding
	while(phy_tx_locked());
	phy_grab_tx_lock();
	//just make sure MAC is not busy
	while(mac_busy());
	//at this point, have the TX lock and MAC is not busy.

	if (dst_addr == 0xffff)		// no ack for broadcaset----chl
		a_mac_tx_data.fcflsb = LRWPAN_FRAME_TYPE_DATA|LRWPAN_FCF_INTRAPAN_MASK;
	else
		a_mac_tx_data.fcflsb = LRWPAN_FRAME_TYPE_DATA|LRWPAN_FCF_INTRAPAN_MASK|LRWPAN_FCF_ACKREQ_MASK;
	a_mac_tx_data.fcfmsb = LRWPAN_FCF_DSTMODE_SADDR|LRWPAN_FCF_SRCMODE_SADDR;

	a_mac_tx_data.DestAddr = dst_addr;
	a_mac_tx_data.SrcAddr = mac_pib.macSAddr;		// Src short addr
//	a_mac_tx_data.SrcPANID = mac_get_PANID();		// no need for short  intra PAN mode
  //  a_mac_tx_data.DestPANID = RADIO_DEFAULT_PANID;	// fixed PANID

	//use the temp space available in the PHY for the TX packet
	//point at end of buffer
	phy_pib.currentTxFrm= &tmpTxBuff[LRWPAN_MAX_FRAME_SIZE];

	for (i =0; i < len; i++) {
		phy_pib.currentTxFrm--;
		*(phy_pib.currentTxFrm) = *(ptr+len-i-1);
	}
	phy_pib.currentTxFlen = len;
	
	a_mac_service.cmd = LRWPAN_SVC_MAC_GENERIC_TX;
	mac_do_service();
	phy_release_tx_lock();

	while(mac_busy())
		mac_FSM();

#ifdef STATISTIC
	
	if (a_mac_service.status == LRWPAN_STATUS_SUCCESS)
		statistic_mac_tx ++;
	else
		statistic_mac_err++;

#endif


	return a_mac_service.status == LRWPAN_STATUS_SUCCESS;
}

