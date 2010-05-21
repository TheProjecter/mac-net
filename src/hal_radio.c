/****************************************************************
 * file:	radio.c
 * date:	2010-05-21
 * description:	radio interface for CC2430
 ***************************************************************/

#include "hal_radio.h"
#include "mac.h"
#include "phy.h"

/************************************************
 	**	Local functions		**
 ***********************************************/
static void radio_init_addr();
static void radio_buffer_init();
static UINT16 radio_free_bytes();

/************************************************
 	**	Local variables		**
 ***********************************************/
#define RADIO_RX_BUFFER_SIZE	(LRWPAN_RXBUFF_SIZE * LRWPAN_MAX_FRAME_SIZE)
static UINT8 radio_rx_buffer[RADIO_RX_BUFFER_SIZE];
static UINT16 radio_rx_head;	// Write empty
static UINT16 radio_rx_tail;	// Read here
static BOOL radio_rx_full;	// Radio rx buffer full?

LRWPAN_STATUS_ENUM radio_init()
{
	UINT16 freq;

	// Initialize radio buffer
	radio_buffer_init();

	freq = 357 + 5 * (RADIO_DEFAULT_CHANNEL - 11);
	FSCTRLL = (UINT8)freq;
	FSCTRLH = ((FSCTRLH &~ 0x03) | (UINT8)((freq >> 8) & 0x03));

	//turning on power to analog part of radio and waiting for voltage regulator.
	RFPWR = 0x04;
	while((RFPWR & 0x10))
          ;

	//corresponds to promiscuous modes
	//MDMCTRL0H &= ~ADR_DECODE;	//no address decode
	//MDMCTRL0L &= ~AUTO_ACK;		//no auto ack

	MDMCTRL0H |= ADR_DECODE;	//auto address decode
	MDMCTRL0L |= AUTO_ACK;		//auto ack

	MDMCTRL0L |= AUTO_CRC;		// Setting for AUTO CRC
	MDMCTRL0H &= ~PAN_COORDINATOR;	//rejects frames with only source addressing modes

	// Turning on AUTO_TX2RX
	FSMTC1 = ((FSMTC1 & (~AUTO_TX2RX_OFF & ~RX2RX_TIME_OFF))  | ACCEPT_ACKPKT);
	FSMTC1 &= ~0x20;		// Turning off abortRxOnSrxon.

	//now configure the RX, TX systems.
	// Setting the number of bytes to assert the FIFOP flag
	IOCFG0 = 127;			//set to max value as the FIFOP flag goes high when complete packet received

	// Flushing both Tx and Rx FiFo. The flush-Rx is issued twice to reset the SFD.
	// Calibrating the radio and turning on Rx to evaluate the CCA.
	SRXON;
	SFLUSHTX;
	SFLUSHRX;
	SFLUSHRX;
	STXCALN;
	ISSTART;
	SACKPEND;			// !!! I don't know whether to use SACK instead

	radio_init_addr();

	//Radio can interrupt when RX configuration clear flags/mask in radio
	RFIF = 0;
	RFIM = 0; 			//all interrupts are masked.

	//enable RX interrupt on processor
	INT_SETFLAG_RF(INT_CLR);
	INT_ENABLE_RF(INT_ON);

	//enable RX RFERR interrupt on processor
	INT_SETFLAG_RFERR(INT_CLR);
	INT_ENABLE_RFERR(INT_ON);

	//do not use DMA at this point, enable the RX receive interrupt here.
	RFIM |= IRQ_FIFOP;
        return(LRWPAN_STATUS_SUCCESS);
}

static void radio_init_addr()
{
	UINT8 buf[8];
#if (CC2430_FLASH_SIZE == 128)
	unsigned char bank;
	bank = MEMCTR;
	//switch to bank 3
	MEMCTR |= 0x30;
#endif
	//note that the flash programmer stores these in BIG ENDIAN order for some reason!!!
	buf[7] = *(unsigned char __code *)(IEEE_ADDRESS_ARRAY+0);
	buf[6] = *(unsigned char __code *)(IEEE_ADDRESS_ARRAY+1);
	buf[5] = *(unsigned char __code *)(IEEE_ADDRESS_ARRAY+2);
	buf[4] = *(unsigned char __code *)(IEEE_ADDRESS_ARRAY+3);
	buf[3] = *(unsigned char __code *)(IEEE_ADDRESS_ARRAY+4);
	buf[2] = *(unsigned char __code *)(IEEE_ADDRESS_ARRAY+5);
	buf[1] = *(unsigned char __code *)(IEEE_ADDRESS_ARRAY+6);
	buf[0] = *(unsigned char __code *)(IEEE_ADDRESS_ARRAY+7);

#if (CC2430_FLASH_SIZE == 128)
	//resore old bank settings
	MEMCTR = bank;
#endif
	IEEE_ADDR0 = buf[0];
	IEEE_ADDR1 = buf[1];
	IEEE_ADDR2 = buf[2];
	IEEE_ADDR3 = buf[3];
	IEEE_ADDR4 = buf[4];
	IEEE_ADDR5 = buf[5];
	IEEE_ADDR6 = buf[6];
	IEEE_ADDR7 = buf[7];
	
	SHORTADDRL = buf[0];	// we use the lower 2 bytes of IEEE addr as short address
	SHORTADDRH = buf[1];
	
	PANIDL =  RADIO_DEFAULT_PANID & 0xFF;
	PANIDH =  (RADIO_DEFAULT_PANID>>8) & 0xFF ;
}

#define PIN_CCA   CCA    //CCA is defined in radio.h
//regardless of what happens here, we will try TXONCCA after this returns.
void do_backoff(void)
{
	UINT8 be, nb, tmp, rannum;
	UINT32  delay, start_tick;

	be = MIN_BE;
	nb = 0;

	do
	{
		if (be)
		{
			//do random delay
			tmp = be;
			//compute new delay
			delay = 1;
			while (tmp)
			{
				delay = delay << 1;	//delay = 2**be;
				tmp--;
			}
			rannum =  hal_get_random_byte() & (delay-1);	//rannum will be between 0 and delay-1
			delay = 0;
			while (rannum)
			{
				delay  += SYMBOLS_TO_MACTICKS(UNIT_BACKOFF_PERIOD);	// default 20 symbols
				rannum--;
			} // delay = aUnitBackoff * rannum
			// now do backoff
			start_tick = mac_timer_get();
			while (MAC_TIMER_NOW_DELTA(start_tick) < delay)
				;
		}
		//check CCA
		if (PIN_CCA)
			break;
		nb++;
		be++;
		if (be > MAX_BE)
			be = MAX_BE;
	}while(nb <= MAX_CSMA_BACKOFFS);
}

/************************************************
 * Description: Use radio to tx packet. The packet
 * 	data is located in mac_header.payload.
 * 	The packet should not longer than 125 bytes.
 * 	Since we mac layer spillted the bigger data
 * 	into packets no longer than 116 bytes. So, we
 * 	just send in this radio_send_packet();
 *
 * Arguments:
 * 	None.
 * Return:
 * 	LRWPAN_STATUS_ENUM--The tx result, SUCCESS/BUSY
 *
 * Date: 2010-05-20
 ***********************************************/
LRWPAN_STATUS_ENUM radio_send_packet()
{
	UINT8 len;		// Packet total length
	LRWPAN_STATUS_ENUM res;	// Send result
	UINT8 *data_pointer = NULL;
	struct data_entry *usr_data = mac_header.payload;

	// Load packet length
	len = MHR_LENGTH + usr_data->p_len + PACKET_FOOTER_SIZE;

	// Clearing RF interrupt flags and enabling RF interrupts.
	if(FSMSTATE == 6 && RXFIFOCNT > 0)
	{
		ISFLUSHRX;
		ISFLUSHRX;
	}

	RFIF &= ~IRQ_TXDONE;		//Clear the RF TXDONE flag
	INT_SETFLAG_RF(INT_CLR);	//Clear processor interrupt flag

	RFD = len;			// Write packet length
	/***********************************************
	 * Write the packet. Use 'flen' as the last two
	 * bytes are added automatically. At some point,
	 * change this to use DMA
	 **********************************************/
	// First, write MAC header
	len = MHR_LENGTH;
	data_pointer = (UINT8 *)&mac_header;
	while (len)
	{
		RFD = *data_pointer++;
		len--;
	}
	// Then, write payload
	len = usr_data->p_len;
	data_pointer = usr_data->data_arr;
	while (len)
	{
		RFD = *data_pointer++;
		len--;
	}

	// If the RSSI value is not valid, enable receiver
	if(RSSIL == 0x80)
	{
		ISRXON;
		// Turning on Rx and waiting 320u-sec to make the RSSI value become valid.
		hal_wait(1);
	}

	do_backoff();
	//Transmitting
	ISTXONCCA;       //TODO: replace this with IEEE Backoff

	if(FSMSTATE > 30)  //is TX active?
	{
		res = LRWPAN_STATUS_SUCCESS;
		RFIM |= IRQ_TXDONE;             //enable IRQ_TXDONE interrupt
	}
	else
	{
		ISFLUSHTX;               //empty buffer
		res = LRWPAN_STATUS_PHY_CHANNEL_BUSY;
		RFIM &= ~IRQ_TXDONE;     //mask interrupt
	}
	return res;
}

//interrupt for RF error
//this interrupt is same priority as FIFOP interrupt,
//but is polled first, so will occur first.
#pragma vector=RFERR_VECTOR
__interrupt static void rf_error_isr()
{
	INT_GLOBAL_ENABLE(INT_OFF);

//	LED_GREEN_TOGGLE();
	// If Rx overflow occurs, the Rx FiFo is reset.
	// The Rx DMA is reset and reception is started over.
	if(FSMSTATE == 17)
	{
		STOP_RADIO();
		ISFLUSHRX;
		ISFLUSHRX;
		ISRXON;
	}
	else if(FSMSTATE == 56)
	{
		ISFLUSHTX;
	}

	INT_SETFLAG_RFERR(INT_CLR);
	INT_GLOBAL_ENABLE(INT_ON);
}

//This interrupt used for both TX and RX
#pragma vector=RF_VECTOR
__interrupt static void rf_isr(void)
{
	UINT8 flen;		// Packet length
	UINT16 current_head;	// Current buffer write index
	UINT8 enabledAndActiveInterrupt;

	INT_GLOBAL_ENABLE(INT_OFF);
	enabledAndActiveInterrupt = RFIF;
	RFIF = 0x00;                        // Clear all radio interrupt flags
	INT_SETFLAG_RF(INT_CLR);    // Clear MCU interrupt flag
	enabledAndActiveInterrupt &= RFIM;
	// complete frame has arrived
	if(enabledAndActiveInterrupt & IRQ_FIFOP)
	{
		// Get packet length
		flen = RFD & 0x7f;  //read the length
		if ( flen >= MHR_LENGTH )
		{
			LED_RED_TOGGLE();	// Debug, used to indicate success receive

			// If we have enough free space, length itself is included
			if (radio_free_bytes() >= flen + 1)
			{
				// Load the entire frame to buffer

				// Keep current radio_rx_head avoid bad CRC involved
				// roll back.
				current_head = radio_rx_head;
				// First, load frame length--this length is not include
				// length byte itself, but include RSSI and CRC bytes.
				radio_rx_buffer[radio_rx_head++] = flen;
				// Load frame
				while( flen )
				{
					// Round up
					if ( radio_rx_head >= RADIO_RX_BUFFER_SIZE )
						radio_rx_head = 0;
					// Write to buffer
					radio_rx_buffer[radio_rx_head++] = RFD;

					// Decrease
					--flen;
				}
				
				// Since we don't know the CRC before we read the RFD,
				// So, the CRC check can't before RxBuffer allocated.
				if ( radio_rx_buffer[(radio_rx_head-1+RADIO_RX_BUFFER_SIZE)%RADIO_RX_BUFFER_SIZE] & 0x80)
				{
					// Ok, we received a good frame
					// CRC good change the RSSI byte from 2's complement to unsigned number
					radio_rx_buffer[(radio_rx_head-2+RADIO_RX_BUFFER_SIZE)%RADIO_RX_BUFFER_SIZE] += 0x80;

					// Update the buffer state
					if ( radio_rx_head == radio_rx_tail )
					{
						radio_rx_full = TRUE;
					}

#ifdef STATISTIC
					statistic_mac_rx++;
#endif
				}
				else
				{
					// Bad CRC, roll back
					radio_rx_head = current_head;
				}
			}
			else
			{
#ifdef STATISTIC
				statistic_mac_drop++;
#endif
			}
		}

		//flush any remaining bytes
		ISFLUSHRX;
		ISFLUSHRX;

		//don't know why, but the RF flags have to be cleared AFTER a read is done.
		RFIF = 0x00;
		INT_SETFLAG_RF(INT_CLR);    // Clear MCU interrupt flag
		//don't know why, but the interrupt mask has to be set again here for some reason.
		//the processor receives packets, but does not generate an interrupt
		RFIM |= IRQ_FIFOP;
	}//end receive interrupt (FIFOP)

	// Transmission of a packet is finished. Enabling reception of ACK if required.
	if(enabledAndActiveInterrupt & IRQ_TXDONE)
	{
		// Finished TX, do call back
		LED_GREEN_TOGGLE();		// DEBUG, sent success

		// Set tx finished flag
		phy_tx_end_callback();
		// Clearing the tx done interrupt enable
		RFIM &= ~IRQ_TXDONE;
	}
	INT_GLOBAL_ENABLE(INT_ON);
}

/************************************************
 *
 *	Radio Rx Buffer Management
 *
 ***********************************************/

/************************************************
 * Description: Init Rx buffer to empty.
 *
 * Arguments:
 * 	None.
 * Return:
 * 	None.
 *
 * Date: 2010-05-21
 ***********************************************/
void radio_buffer_init()
{
	radio_rx_head = 0;
	radio_rx_tail = 0;
	radio_rx_full = FALSE;
}

/************************************************
 * Description: Get the buffer's state--Full/Empty
 *
 * Arguments:
 * 	None.
 * Return:
 * 	TRUE -- If radio rx buffer is full
 * 	FALSE -- If not
 *
 * Date: 2010-05-21
 ***********************************************/
BOOL radio_buffer_full()
{
	return radio_rx_full;
}

/************************************************
 * Description: Get the buffer's state--Full/Empty
 *
 * Arguments:
 * 	None.
 * Return:
 * 	TRUE -- If radio rx buffer is empty
 * 	FALSE -- If not
 *
 * Date: 2010-05-21
 ***********************************************/
BOOL radio_buffer_empty()
{
	return ( radio_rx_full == FALSE && radio_rx_head == radio_rx_tail );
}

/************************************************
 * Description: Get a byte from radio rx buffer
 * 	at [index], but NOT remove the byte.
 *
 * Arguments:
 * 	index -- Indicate the index in radio rx buffer
 * Return:
 * 	UINT8 -- Byte content at [index]
 *
 * Date: 2010-05-21
 ***********************************************/
UINT8 radio_get_byte( UINT16 index )
{
	return radio_rx_buffer[index%RADIO_RX_BUFFER_SIZE];
}

/************************************************
 * Description: Get the first frame's length.
 *
 * Arguments:
 * 	None.
 * Return:
 * 	UINT8 -- The first frame's length
 *
 * Date: 2010-05-21
 ***********************************************/
UINT8 radio_get_frm_length()
{
	return radio_rx_buffer[radio_rx_tail];
}

/************************************************
 * Description: Move the radio_rx_tail index to
 * 	[radio_rx_tail + num] for drop num bytes in buffer.
 * 	We assume the user has ensured the index's range
 *
 * Arguments:
 *	num -- Number of dropped bytes
 * Return:
 * 	None.
 *
 * Date: 2010-05-21
 ***********************************************/
void radio_drop_bytes(UINT8 num)
{
	// Move the read pointer index to [radio_tx_tail + num]
	radio_rx_tail = (radio_rx_tail + num)%RADIO_RX_BUFFER_SIZE;
	// Update state
	radio_rx_full = FALSE;
}

/************************************************
 * Description: The free space(bytes) in radio rx
 * 	buffer.
 *
 * Arguments:
 * 	None.
 * Return:
 * 	UINT16 -- The number of bytes free for writing.
 *
 * Date: 2010-05-21
 ***********************************************/
UINT16 radio_free_bytes()
{
	if ( radio_buffer_empty() )
	{
		return RADIO_RX_BUFFER_SIZE;
	}
	return (radio_rx_head - radio_rx_tail + RADIO_RX_BUFFER_SIZE)%RADIO_RX_BUFFER_SIZE;
}
