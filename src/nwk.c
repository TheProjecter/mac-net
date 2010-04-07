/*
 * file: nwk.c
 * date: 2009-11-04
 * description: nwk layer
 */

#include "nwk.h"
#include "mac.h"
#include "queue.h"
#include "trans.h"
#include "dsr.h"
#include "route_cache.h"
#include "hal_timer.h"



// #define DEBUG

static void nwk_RX();
static void nwk_TX();
static void handle_request(nwk_opt_t *opt);
static void handle_reply(nwk_opt_t *opt);
static void handle_error(nwk_opt_t *opt);
static void handle_data(nwk_opt_t *opt, UINT8 *ppayload, UINT8 mpdu_len);
static void rt_populate(nwk_opt_t * opt, struct rt_entry *p_route );

struct queue nwk_send_queue;		
static UINT8 last_bcast_id = 0;	// to avoid broadcast twice
static UINT8 last_req_id = 0;

#ifdef DEBUG
static void nwk_opt_dump(nwk_opt_t *opt);
#endif

void nwk_init()
{
	mac_init();
	queue_init(&nwk_send_queue);
	rt_cache_init();
}

void nwk_main()
{
	nwk_RX();		// fetch packet from mac layer
	nwk_TX();		// fetch frame from transport layer
}

static void nwk_RX()
{
	UINT8 *ptr = NULL;
	UINT8 paylen;
//	UINT8 i;
	nwk_opt_t* opt;

	static UINT8 last_len  = 0;
	static UINT8 last_seq = 0;
	static UINT8 last_mac_saddr_lsb =  0;
	static UINT8 last_mac_saddr_msb = 0;


	if (mac_rx_buff_empty())
		return;
	
	ptr = mac_get_rx_packet();


	if ( ptr )
	{

		// TODO: implement this in MAC layer makes more sense
		if ( (*ptr == last_len) && (*(ptr+3) == last_seq) && (*(ptr+8) == last_mac_saddr_lsb)
				&& (*(ptr+9) == last_mac_saddr_msb) ) {
			// shit, we received duplicated packet!
			mac_free_rx_packet();
#ifdef STATISTIC
			statistic_mac_rx--;
#endif

		last_len = *ptr;			// this is not necessary!, because they did not change
		last_seq = *(ptr+3);
		last_mac_saddr_lsb = *(ptr+8);
		last_mac_saddr_msb = *(ptr+9);
			return;

		}
		last_len = *ptr;
		last_seq = *(ptr+3);
		last_mac_saddr_lsb = *(ptr+8);
		last_mac_saddr_msb = *(ptr+9);

		paylen = *ptr - 9 - MAC_DATA_RSSI_CRC;	// nwk header and payload  len, that is mpdu's len

		opt = (nwk_opt_t *)(ptr+10);

#ifdef DEBUG
		nwk_opt_dump(opt);
#endif

		switch(opt->type) {
			case ROUTE_REQ:
				handle_request(opt);
				break;
			case ROUTE_REPLY:
				handle_reply(opt);
				break;
			case ROUTE_ERROR:
				handle_error(opt);
				break;
			case ROUTE_DATA:
				handle_data(opt, ptr+10, paylen);
				break;
			default:
				break;

//		trans_fram_assemble(src_addr, ptr+10, len);
		/*
		uart_putch('\r');
		for (i = 0; i < len; i++) {
			uart_putch(*(ptr+10+i));
		}
		uart_putch('\r');
		*/
		}
	mac_free_rx_packet();
	}
}


/*
 * fetch packet from queue, and send it
 */
static void nwk_TX()
{
	UINT16 dst_addr;
	struct data_entry * queue_data;
	struct rt_entry *p_route = NULL;
	nwk_opt_t * p_opt;
	UINT8 rt_index;


	if (! queue_is_empty(&nwk_send_queue)) {
		queue_data = queue_get_next(&nwk_send_queue);
		dst_addr = queue_data->dst_addr;
		p_opt = (nwk_opt_t *)queue_data->data_arr;

		// special case for broadcast
		if (dst_addr == 0xffff) {
			p_opt->type = ROUTE_DATA;
			p_opt->hops = MAX_ROUTE_LEN;		// when broadcast, hops is used as ttl(time to live)
			p_opt->taddr = 0xffff;
			p_opt->bcast_id = hal_get_random_byte();	// random
			p_opt->addr[0] = mac_get_addr();

			last_bcast_id = p_opt->bcast_id;

			if (mac_do_send(dst_addr, queue_data->data_arr, sizeof(nwk_opt_t) + queue_data->p_len)) {
#ifdef DEBUG
				uart_putstr("bcast ok\r");
#endif		

#ifdef STATISTIC
				statistic_data_tx++;
#endif
			} else {
#ifdef DEBUG
				uart_putstr("bcast fail\r");
#endif
			}

			queue_remove_next(&nwk_send_queue);
			return;
		}

		if ( (p_route = rt_lookup_route(dst_addr)) != NULL) {
			// route available, send it, and remove from queue

			// TODO: check if hops exceed MAX_ROUTE_LEN
			if (p_route->segs_left > MAX_ROUTE_LEN) {
#ifdef 	DEBUG
			uart_putstr("segs exceed\r");
#endif
			queue_remove_next(&nwk_send_queue);
			return;
			}


			p_opt->cur_len = 1;
			rt_populate(p_opt, p_route);

			if(p_opt->hops == 1)
				dst_addr = p_opt->taddr;			// if one hop, the next hop is the dest addr
			else
				dst_addr = p_opt->addr[1];	// get next hop addr from the array

			if(mac_do_send(dst_addr, queue_data->data_arr, sizeof(nwk_opt_t) + queue_data->p_len)) {
#ifdef DEBUG
				uart_putstr("sent data,ok\r");	// debug
#endif
#ifdef STATISTIC
				statistic_data_tx++;
#endif
				;
			} else {
#ifdef DEBUG
				uart_putstr("sent data,fail\r");	// debug
				uart_puthex(a_mac_service.status);
				uart_putch('\r');
#endif

				// update_the route
				rt_index = rt_lookup_cache_index(queue_data->dst_addr);
				if (rt_index < ROUTE_CACHE_SIZE)
					cache_entry_expired |= (1<<rt_index);
#ifdef DEBUG
				rt_dump();	// debug
#endif
			}	

			queue_remove_next(&nwk_send_queue);
			return;
		} else if (queue_data->req_time == 0) {
			// first time, send ROUTE REQ
			
			p_opt->type = ROUTE_REQ;
			p_opt->ident = hal_get_random_byte();	// shall we maintain a sequence and inc every time ?
			p_opt->taddr = dst_addr;
			p_opt->cur_len = 1;
			p_opt->addr[0] = mac_get_addr();
			last_req_id = p_opt->ident;
			if(mac_do_send(0xffff, queue_data->data_arr, sizeof(nwk_opt_t))) {
#ifdef DEBUG
				uart_putstr("REQ sent ok\r");
#endif
			} else {
#ifdef DEBUG
				uart_putstr("REQ sent fail\r");
#endif
			}
			queue_data->req_time = mac_timer_get();	// record the request sent time
			return;

		} else if (MAC_TIMER_NOW_DELTA(queue_data->req_time) > MSECS_TO_MACTICKS(REQ_WATIT_TIME)) {
			// no reply received , remove the queue
			queue_remove_next(&nwk_send_queue);
#ifdef STATISTIC				
				statistic_data_drop++;
#endif
			return;
		} else {
			// continue to wait for reply
			return;
		}

	}

}

/*
 * handle incoming route request
 *
 * opt->cur_len means current nodes on the path, including the requester ,excluding myself
 * so it also means hops to the requester
 */
static void handle_request(nwk_opt_t *opt)
{
	struct rt_entry * ptr = NULL;
	UINT8  len, i;
	UINT16 my_addr;
	volatile UINT16 delay;

	if (opt->ident == last_req_id)
		return;		// ignore if receive the same request packet

	my_addr = mac_get_addr();
	for (i = 0; i < opt->cur_len; i++)
		if (opt->addr[i] == my_addr)	// already in the path
			return;

	last_req_id = opt->ident;	// update the last request id

	if (opt->taddr == my_addr) {
		// this is for me
		opt->hops = opt->cur_len;
		rt_update_reverse_cache(opt);

		// delay randome time

		opt->type = ROUTE_REPLY;
		opt->cur_len --;

		// now, opt->addr[opt->cur_len] is the next dest this reply will go
#ifdef DEBUG
		uart_putstr("send REPLY1 ");
#endif
		if (mac_do_send(opt->addr[opt->cur_len], (UINT8 *)opt, sizeof(nwk_opt_t))) {
#ifdef DEBUG
			uart_putstr("ok\r");
#endif
		} else {
#ifdef DEBUG
			uart_putstr("fail\r");
#endif
		}
	
	}
	else if (ptr = rt_lookup_route(opt->taddr)) {
		// this node update it's route and return a reply packet
		len = opt->cur_len;
		opt->hops = len + ptr->segs_left;
		if (opt->hops > MAX_ROUTE_LEN)
			return;
		for ( i = 0; i < ptr->segs_left; i++ )
		{
			opt->addr[i+len] = ptr->addr[i];
		}
		rt_update_reverse_cache(opt);

		// Delay for a random time
					//nwk_delay_send_reply(rt_req_opt.ident_hops);

					// send back the RT_REP packet
		opt->type = ROUTE_REPLY;
		opt->cur_len --;
#ifdef DEBUG
		uart_putstr("send REPLY2 ");
#endif
		if (mac_do_send(opt->addr[opt->cur_len], (UINT8 *)opt, sizeof(nwk_opt_t))) {
#ifdef DEBUG
			uart_putstr("ok\r");
#endif
		} else {
#ifdef DEBUG
			uart_putstr("fail\r");
#endif
		}
		
	} else {
		// neither this node is the dst, nor route cache has, then forward the req
		//
		// TODO: shall we update reverse route cache ?
		if (opt->cur_len < MAX_ROUTE_LEN) {
			opt->addr[opt->cur_len] = mac_get_addr();
			opt->cur_len++;
		} else {
			return;
		}
		last_req_id = opt->ident;	// update the id
#ifdef DEBUG
		uart_putstr("fwd REQ ");
#endif
		delay = hal_get_random_byte() + 20;
		while (delay)
			delay--;
		if (mac_do_send(0xffff, (UINT8 *)opt, sizeof(nwk_opt_t))) {
#ifdef DEBUG
			uart_putstr("ok\r");
#endif
		} else {
#ifdef DEBUG
			uart_putstr("fail\r");
#endif
		}
	}
		
}

/*
 * this handle the incoming reply packet
 * now, opt->cur_len is the hops to the requester
 */
static void handle_reply(nwk_opt_t * opt) {
//  volatile UINT16 delay;

	
	if ( 0 == opt->cur_len) {
		// the REPLY returns to the src Address
		rt_update_forward_cache(opt);
#ifdef DEBUG
		rt_dump();	// debug
#endif
	} else if (opt->cur_len > 0) {
		rt_update_forward_cache(opt);
		rt_update_reverse_cache(opt);
#ifdef DEBUG
		rt_dump();	// debug
#endif

		// forward the REPLY
		opt->cur_len -= 1;
#ifdef DEBUG
		uart_putstr("fwd REPLY ");
#endif
/*
			delay = hal_get_random_byte() + 250;
		while (delay)
			delay--;
			*/

		if (mac_do_send(opt->addr[opt->cur_len], (UINT8 *)opt, sizeof(nwk_opt_t))) {
#ifdef DEBUG
			uart_putstr("ok\r");
#endif
		} else {
#ifdef DEBUG
			uart_putstr("fail\r");
#endif
		}
	}
}

/*
 * handle the incoming ROUTE_ERROR packet
 */
static void handle_error(nwk_opt_t *opt)
{
	UINT8 res;
   //     volatile UINT16 delay;

		// TODO: compare the found route in rt_cache to the route in the opt,
		// 		 only invalidate if the path is same.
	res = rt_lookup_cache_index(opt->taddr);
	if (res < ROUTE_CACHE_SIZE)
		cache_entry_expired |= 1 << res;
#ifdef DEBUG
	rt_dump();
#endif

	if (0 == opt->cur_len) {
		// the ERROR return to the source node
	
		;
	} else if (opt->cur_len > 0) {
		// forward the ERROR to previous node (on backward path)
		opt->cur_len -= 1;
#ifdef DEBUG
		uart_putstr("fwd ERR ");
#endif
		/*
		delay = hal_get_random_byte() + 50;
		while (delay)
			delay--;
			*/
		if (mac_do_send(opt->addr[opt->cur_len], (UINT8 *)opt, sizeof(nwk_opt_t))) {
#ifdef DEBUG
			uart_putstr("ok\r");
#endif
		} else {
#ifdef DEBUG
			uart_putstr("fail\r");
#endif
		}
	}

}


/*
 * this handles incoming data packet
 *
 */
static void handle_data(nwk_opt_t * opt, UINT8 *p_mac_load, UINT8 mpdu_len)
{
	UINT16 dst_addr;
//	volatile	UINT16 delay;
	UINT8 rt_index;


	// special case for broadcast
	if (opt->taddr == 0xffff) {
		if ((last_bcast_id == opt->bcast_id) || (opt->hops == 0) ||
				   	(opt->addr[0] == mac_get_addr()) )	// received once or ttl exhausted
			return;
		else {

			trans_fram_assemble(opt->addr[0], p_mac_load+sizeof(nwk_opt_t), mpdu_len-sizeof(nwk_opt_t));

			/*
			for (i = 0; i < mpdu_len - sizeof(nwk_opt_t); i++)
				uart_putch(*(p_mac_load + sizeof(nwk_opt_t) + i));
			uart_putch('\r');
			*/

			last_bcast_id = opt->bcast_id;
			// re broadcast it
			opt->hops--;

			// TODO: shall we delay first ?
			if (mac_do_send(0xffff, p_mac_load, mpdu_len)) {
#ifdef DEBUG
				uart_putstr("rbcast ok\r");
#endif
			} else {
#ifdef DEBUG
				uart_putstr("rbcast fail\r");
#endif
			}
			return ;
		}
	}


	if (opt->taddr == mac_get_addr()) {
		trans_fram_assemble(opt->addr[0], p_mac_load+sizeof(nwk_opt_t), mpdu_len-sizeof(nwk_opt_t));
		return;

		/*
		for (i = 0; i < mpdu_len - sizeof(nwk_opt_t); i++) {
			uart_putch(*(p_mac_load + sizeof(nwk_opt_t) + i));
		}
		uart_putch('\r');
		*/
	} else {
		if (opt->cur_len == opt->hops - 1)	// next hop is destination
			dst_addr = opt->taddr;
		else
			dst_addr = opt->addr[opt->cur_len + 1];
		opt->cur_len ++;
#ifdef DEBUG
		uart_putstr("fwd DAT ");
#endif

		/*
		delay = hal_get_random_byte() + 250;
		while (delay)
			delay--;
			*/


		if (mac_do_send(dst_addr, p_mac_load, mpdu_len)) {
#ifdef DEBUG
			uart_putstr("ok\r");
#endif
		}else {
#ifdef DEBUG
			uart_putstr("fail\r");
#endif

			// send ROUTE packet
			opt->type = ROUTE_ERROR;
			opt->cur_len -= 1;			// re adjust
			opt->hops = opt->cur_len;
			opt->cur_len --;
#ifdef DEBUG
			uart_putstr("send ERR ");
#endif


			if(mac_do_send(opt->addr[opt->cur_len], (UINT8 *)opt, sizeof(nwk_opt_t))) {
#ifdef DEBUG
				uart_putstr("ok\r");
#endif
			} else {
#ifdef DEBUG
				uart_putstr("fail\r");
#endif

			}
				// update_the route
			rt_index = rt_lookup_cache_index(opt->taddr);
			if (rt_index < ROUTE_CACHE_SIZE)
				cache_entry_expired |= (1<<rt_index);
#ifdef DEBUG
			rt_dump();	// debug
#endif

		}
	}
}

/*
 * populate route info to network header
 */
static void rt_populate(nwk_opt_t * opt, struct rt_entry *p_route )
{
    UINT8 i;

	opt->type = ROUTE_DATA;
	opt->hops = p_route->segs_left;
	opt->taddr = p_route->dst;
	for ( i = 0; i < opt->hops; i++ )
	{
		opt->addr[i] = p_route->addr[i];
	}
}

#ifdef DEBUG
static void nwk_opt_dump(nwk_opt_t *opt)
{
	UINT8 i;
	UINT8 *p;

	p = (UINT8 *)opt;
	for (i = 0; i < sizeof(nwk_opt_t); i++) {
		uart_puthex(*(p+i));
		uart_putch(' ');
	}
	uart_putch('\r');
}
#endif	
