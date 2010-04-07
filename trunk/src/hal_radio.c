/****************************************************************
 * file:	radio.c
 * date:	2009-03-13
 * description:	radio interface for CC2430
 ***************************************************************/

#include "hal_radio.h"
#include "mac.h"
#include "phy.h"

LRWPAN_STATUS_ENUM radio_init()
{
	UINT16 freq;

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

//transmit packet: flen should be less than 125
LRWPAN_STATUS_ENUM radio_send_packet(UINT8 flen, UINT8 *frm)
{
	UINT8 len;
	LRWPAN_STATUS_ENUM res;

	//total length, does not include length byte itself
	//last two bytes are the FCS bytes that are added automatically
	len = flen + PACKET_FOOTER_SIZE;

	if (len > 127)
	{
		//packet size is too large!
		return(LRWPAN_STATUS_PHY_TX_PKT_TOO_BIG);
	}

	// Clearing RF interrupt flags and enabling RF interrupts.
	if(FSMSTATE == 6 && RXFIFOCNT > 0)
	{
		ISFLUSHRX;
		ISFLUSHRX;
	}

	RFIF &= ~IRQ_TXDONE;      //Clear the RF TXDONE flag
	INT_SETFLAG_RF(INT_CLR);  //Clear processor interrupt flag

	//write packet length
	RFD = len;
	//write the packet. Use 'flen' as the last two bytes are added automatically
	//At some point, change this to use DMA
	while (flen)
	{
		RFD = *frm++;
		flen--;
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
		// Asserting the status flag and enabling ACK reception if expected.
		phy_tx_start_callback();
		res = LRWPAN_STATUS_SUCCESS;
		RFIM |= IRQ_TXDONE;             //enable IRQ_TXDONE interrupt
	}
	else
	{
		ISFLUSHTX;               //empty buffer
		res = LRWPAN_STATUS_PHY_CHANNEL_BUSY;
		RFIM &= ~IRQ_TXDONE;     //mask interrupt
	}
	return(res);
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
//	UINT8 i, j;		// debug

	UINT8 flen;
	UINT8 enabledAndActiveInterrupt;
	UINT8 *ptr, *rx_frame;
	UINT8 ack_bytes[5];
	UINT8 crc;
	//define alternate names for readability in this function
#define  fcflsb ack_bytes[0]
#define  fcfmsb ack_bytes[1]
#define  dstmode ack_bytes[2]
#define  srcmode ack_bytes[3]

	
	INT_GLOBAL_ENABLE(INT_OFF);
	enabledAndActiveInterrupt = RFIF;
	RFIF = 0x00;                        // Clear all radio interrupt flags
	INT_SETFLAG_RF(INT_CLR);    // Clear MCU interrupt flag
	enabledAndActiveInterrupt &= RFIM;
	// complete frame has arrived
	if(enabledAndActiveInterrupt & IRQ_FIFOP)
	{
		//if last packet has not been processed, we just
		//read it but ignore it.
		ptr = NULL; //temporary pointer
		flen = RFD & 0x7f;  //read the length
		if (flen == LRWPAN_ACKFRAME_LENGTH)
		{
			//this should be an ACK. read the packet
			ack_bytes[0]= flen;
			ack_bytes[1] = RFD;	//LSB Frame Control Field
			ack_bytes[2] = RFD;	//MSB Frame Control Field
			ack_bytes[3] = RFD;	//dsn
			ack_bytes[4] = RFD;	//RSSI
			crc = RFD;
			//check CRC
			if (crc & 0x80)
			{
				// CRC ok, perform callback if this is an ACK
				macRxCallback(ack_bytes, ack_bytes[4]);
			}

		}
		else
		{
			//read the fcflsb, fcfmsb
			fcflsb = RFD;
			fcfmsb = RFD;

			if (!mac_rx_buff_full())
			{
				//MAC TX buffer has room allocate new memory space
				//read the length
				rx_frame = mac_rx_frames[mac_pib.rxHead];

				mac_pib.rxHead++;
				if (mac_pib.rxHead >= LRWPAN_RXBUFF_SIZE)
					mac_pib.rxHead = 0;
				if (!(mac_pib.rxFull) && mac_pib.rxTail == mac_pib.rxHead)
					mac_pib.rxFull = TRUE;

				ptr = rx_frame;
			}
#ifdef STATISTIC
			else
			{
				statistic_mac_drop++;
			}
#endif

			// at this point, if ptr is null, then either
			// the MAC RX buffer is full or there is  no
			// free memory for the new frame, or the packet is
			// going to be rejected because of addressing info.
			// In these cases, we need to
			// throw the RX packet away
			if (ptr == NULL)
			{
				//just flush the bytes
				goto do_rxflush;
			}else
			{

			LED_RED_TOGGLE();	// debug use to indicate success receive

				//save packet, including the length
				*ptr = flen; ptr++;

		//		j = flen;		// debug

				//save the fcflsb, fcfmsb bytes
				*ptr = fcflsb; ptr++; flen--;
				*ptr = fcfmsb; ptr++; flen--;
				//get the rest of the bytes
				while (flen) { *ptr = RFD;  flen--; ptr++; }
			
		/*	
				// for debug
				uart_putstr("r: ");
				for(i = 0; i < j; i++) {
					uart_puthex(*(rx_frame+i));
					uart_putch(' ');
				}
			
*/
				//do RX callback
				//check the CRC
				if (*(ptr-1) & 0x80)
				{
					//CRC good
					//change the RSSI byte from 2's complement to unsigned number
					*(ptr-2) = *(ptr-2) + 0x80;
#ifdef STATISTIC
					statistic_mac_rx++;
#endif
					//phyRxCallback();
					macRxCallback(rx_frame, *(ptr-2));
					usrIntCallback();
				} else {
					// crc bad, remove from the head

					if (mac_pib.rxHead == 0)
						mac_pib.rxHead = LRWPAN_RXBUFF_SIZE - 1;
					else 
						mac_pib.rxHead --;

					mac_pib.rxFull  = FALSE;

				}


			}
		}

		//flush any remaining bytes
do_rxflush:
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
		//Finished TX, do call back
		LED_GREEN_TOGGLE();		// debug, sent success
//		uart_putch('F');
		phy_tx_end_callback();
		macTxCallback();
		// Clearing the tx done interrupt enable
		RFIM &= ~IRQ_TXDONE;
	}
	INT_GLOBAL_ENABLE(INT_ON);

#undef  fcflsb
#undef  fcfmsb
#undef  dstmode
#undef  srcmode
}
