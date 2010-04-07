/*
 * file: route_cache.c
 * date: 2009-11-04
 * description: route cache management
 */

#include "route_cache.h"
#include "mac.h"

static struct rt_entry rt_cache[ROUTE_CACHE_SIZE];
static UINT8 cache_head;		// use FIFO cache replace strategy
static UINT8 cache_tail;
static BOOL cache_full;
UINT8 cache_entry_expired;

void rt_cache_init()
{
	// UINT8 i, j;
	cache_head = cache_tail = 0;
	cache_full = FALSE;
	cache_entry_expired = 0;

	/*
	for (i = 0; i < ROUTE_CACHE_SIZE; i++) {
		rt_cache[i].segs_left = 0;
		for (j = 0; j < MAX_ROUTE_LEN; j++)
			rt_cache[i].addr[j] = 0;
		rt_cache[i].dst = 0;
	}
	*/

}

/*
 * check whether the route in nwk_opt exist in our cache table
 */
static BOOL rt_cache_exist(nwk_opt_t* opt, BOOL reverse)
{
	// Return the index of the cache entry whose dst is rt_req->taddr
	UINT8 i, j, cur_index;
	UINT16 taddr;
	BOOL has_exist = FALSE;

	if ( reverse )
	{
		taddr = opt->addr[0];
		cur_index = opt->cur_len;

		for ( i=0; i < ROUTE_CACHE_SIZE; i++ )
		{
			if ( rt_cache[i].dst == taddr && 0==(cache_entry_expired&(1<<i)) )
			{
				// Compare route hops
				if ( rt_cache[i].segs_left == cur_index )
				{
					// Compare route node
					j = 1;		// i = 0, is this node's address, ignore it
					while( j < cur_index )
					{
						if (rt_cache[i].addr[j] == opt->addr[cur_index-j])
						{
							j++;
						}
						else
						{
							// Two different route with the same hops.
							// For multi-route, add to the cache.
							break;
						}
					}
					if (j == cur_index )
					{
						has_exist = TRUE;
						break;
					}
				}
			}
		}
		if ( ROUTE_CACHE_SIZE == i )
		{
			has_exist = FALSE;
		}
	} // end for -- if ( reverse )
	else
	{
		taddr = opt->taddr;
		cur_index = opt->hops - opt->cur_len;

		for ( i=0; i < ROUTE_CACHE_SIZE; i++ )
		{
			if ( rt_cache[i].dst == taddr && 0==(cache_entry_expired&(1<<i)) )
			{
				// Compare route hops
				if ( rt_cache[i].segs_left == cur_index )
				{
					// Compare route node
					while( j < cur_index )
					{
						if (rt_cache[i].addr[j] == opt->addr[opt->cur_len + j])
						{
							j++;
						}
						else
						{
							// Two different route with the same hops.
							// For multi-route, add to the cache.
							break;
						}
					}
					if (j == cur_index )
					{
						has_exist = TRUE;
						break;
					}
				}
			}
		}
		if ( ROUTE_CACHE_SIZE == i )
		{
			has_exist = FALSE;
		}
	} // end for -- else
	return has_exist;
}

UINT8 rt_lookup_cache_index(UINT16 taddr)
{
	// Return the index of the cache entry whose dst is rt_req->taddr
	UINT8 i;
	UINT8 temp = ROUTE_CACHE_SIZE;

	for ( i=0; i < ROUTE_CACHE_SIZE; i++ )
	{
		if ( rt_cache[i].dst==taddr && 0==(cache_entry_expired&(1<<i)) )
		{
			// find the first available element
			if ( ROUTE_CACHE_SIZE == temp )
			{
				temp = i;
			}
			// Compare the hops, chose the smaller one
			else if ( rt_cache[i].segs_left < rt_cache[temp].segs_left )
			{
				temp = i;
			}
		}
	}
        return temp;
}

// look up a route and clean out any expired routes if a route
// is found it is set as new if no route is found NULL is returned
struct rt_entry *rt_lookup_route(UINT16 dst)
{
	// no timeout Cache used,added later.
	UINT8 i;
	struct rt_entry * rt_ptr = NULL;

	/*
	if ( !nwk_data_forward )
	{
		rt_req_opt.cur_len = 0;					/// bad design
	}
	// rt_req_opt.cur_len must less than MAX_ROUTE_LEN
	if ( rt_req_opt.cur_len >= MAX_ROUTE_LEN )
	{
		return NULL;
	}
	*/

	// brute force search for now, hash table later
	// the most current entry with the smallest hop count will be used
	for (i = 0;i < ROUTE_CACHE_SIZE;i++)
	{
	
			if ( rt_cache[i].dst == dst && 0==(cache_entry_expired&(1<<i)) )
			{
				/* don't check it here ! when populate the nwk header, check then--- chl
				// Sorry! The src node can only hold MAX_ROUTE_LEN entries
				if ( rt_cache[i].segs_left + rt_reqopt.cur_len > MAX_ROUTE_LEN )
				{
					continue;
				}
				*/

				/* found an entry */
				if (NULL == rt_ptr)
					rt_ptr = rt_cache + i;
				else if (rt_cache[i].segs_left < rt_ptr->segs_left)
					rt_ptr = rt_cache + i;
			}
	
	}
	return rt_ptr;
}

void rt_update_forward_cache(nwk_opt_t* opt)
{
	UINT8 i = 0;
	UINT8 cur_index = opt->cur_len;

	// First: lookup the cache to see if this route already exit?
	if ( rt_cache_exist(opt, FALSE) )
	{
//		uart_putstr("\rrt exist\r");
		return;
	}
//	uart_putstr("\rnew f rt\r");

	// update the node's route cache
	if ( !cache_full )
	{
		// add directly, copy route to cache -- forward route
		rt_cache[cache_tail].segs_left = opt->hops - cur_index;
		rt_cache[cache_tail].dst = opt->taddr;
		while( i < rt_cache[cache_tail].segs_left )
		{
			// the route including itself
			rt_cache[cache_tail].addr[i] = opt->addr[cur_index+i];
			i++;
		}
		cache_tail++;
		if ( cache_tail >= ROUTE_CACHE_SIZE )
		{
			cache_tail = cache_tail - ROUTE_CACHE_SIZE;
		}
		if (cache_tail == cache_head )
		{
			// cache full the first time, then it will full untill terminated because
			// no cache entry 'time out' strategy is introduced.
			cache_full = TRUE;
		}
	}
	// Check if have some expired entry
	else if(cache_entry_expired)
	{
		UINT8 j = 0;
		// The first expired entry from low to high bits
		while( 0==(cache_entry_expired & (1<<j)) )
		{
			j++;
		}
		rt_cache[j].segs_left = opt->hops - cur_index;
		rt_cache[j].dst = opt->taddr;
		while( i < rt_cache[j].segs_left )
		{
			// the route including itself
			rt_cache[j].addr[i] = opt->addr[cur_index+i];
			i++;
		}
                cache_entry_expired &= ~(1<<j);
	}
	// Really full!
	else
	{
		// replace the cache_head cache entry, implement LRU later??????
		rt_cache[cache_head].segs_left = opt->hops - cur_index;
		rt_cache[cache_head].dst = opt->taddr;
		while( i < rt_cache[cache_head].segs_left )
		{
			// the route including itself
			rt_cache[cache_head].addr[i] = opt->addr[cur_index+i];
			i++;
		}
		cache_head++;
		if ( cache_head >= ROUTE_CACHE_SIZE )
		{
			cache_head = cache_head - ROUTE_CACHE_SIZE;
		}
	}
	// for dubeg only, delete if run on real mouse ??????
//	uart_putch('X');
//	conPrintUINT8_noleader(rt_req->ident_hops);
  //      conPrintUINT8_noleader(cur_index);
}

void rt_update_reverse_cache(nwk_opt_t* opt)
{
	// Warning: rt_req is read only
	UINT8 i = 0;
	UINT8 cur_index = opt->cur_len;

	if ( cur_index < 1 )
	{
		// Don't add reverse route if this is the src node itself
		return;
	}

	// First: lookup the cache to see if this route already exit?
	if ( rt_cache_exist(opt, TRUE) )
	{	
//		uart_putstr("\rrt exist\r");
		return;
	}
//	uart_putstr("\rnew brt\r");

	// update the node's route cache
	if ( !cache_full )
	{

		// add directly, copy route to cache --nwk_data_forward forward route
		rt_cache[cache_tail].segs_left = cur_index;
		rt_cache[cache_tail].dst = opt->addr[0];	// Reverse: dst address is the src node
		// the route including itself
		i = 0;
		rt_cache[cache_tail].addr[i++] = mac_get_addr();
		while( i < cur_index )
		{
			rt_cache[cache_tail].addr[i] = opt->addr[cur_index-i];
			i++;
		}
		cache_tail++;
		if ( cache_tail >= ROUTE_CACHE_SIZE )
		{
			cache_tail = cache_tail - ROUTE_CACHE_SIZE;
		}
		if (cache_tail == cache_head )
		{
			// cache full the first time, then it will full untill terminated because
			// no cache entry 'time out' strategy is introduced.
			cache_full = TRUE;
		}
	}
	// Check if have some expired entry
	else if(cache_entry_expired)
	{
		UINT8 j = 0;
		// The first expired entry from low to high bits
		while( 0 == (cache_entry_expired & (1<<j)) )
		{
			j++;
		}
		rt_cache[j].segs_left = cur_index;
		rt_cache[j].dst = opt->addr[0];
		// the route including itself
		i = 0;
		rt_cache[j].addr[i++] = mac_get_addr();
		while( i < cur_index )
		{
			// the route including itself
			rt_cache[j].addr[i] = opt->addr[cur_index-i];
			i++;
		}
		cache_entry_expired &= ~(1<<j);
	}
	// Really full!
	else
	{
		// replace the cache_head cache entry, implement LRU later??????
		rt_cache[cache_head].segs_left = cur_index;
		rt_cache[cache_head].dst = opt->addr[0];
		// the route including itself
		i = 0;
		rt_cache[cache_head].addr[i++] = mac_get_addr();
		while( i < cur_index )
		{
			// the route including itself
			rt_cache[cache_head].addr[i] = opt->addr[cur_index-i];
			i++;
		}
		cache_head++;
		if ( cache_head >= ROUTE_CACHE_SIZE )
		{
			cache_head = cache_head - ROUTE_CACHE_SIZE;
		}
	}
	// for dubeg only, delete if run on real mouse ??????
//	uart_putch('V');
}

/*
 * this dump the route table info
 */
void rt_dump()
{
	UINT8 i;
	UINT8 j;
	struct rt_entry *ptr = NULL;

	uart_puthex(cache_entry_expired);
	uart_putch('\r');

	/*
	// print valid route entry
	for (i = 0; i < 8; i++) {
		if ((cache_entry_expired&(i<<i)) == 0) {
			ptr = rt_cache + i;
			j = 0;
			while (j < ptr->segs_left) {
				uart_puthex(ptr->addr[j] >> 8);
				uart_puthex(ptr->addr[j] & 0xff);
				uart_putstr("-->");
				j++;
			}
            uart_puthex(ptr->dst >> 8);
            uart_puthex(ptr->dst & 0xff);
			
			uart_putch('\r');
		}
		;
	}
	*/
				

	for ( i = 0; i < ROUTE_CACHE_SIZE; i++ )
	{
		ptr = rt_cache + i;
	
			j = 0;
			while ( j < ptr->segs_left )
			{
				uart_puthex(ptr->addr[j] >> 8);
				uart_puthex(ptr->addr[j] & 0xff);
				uart_putstr("->");
				j++;
			}
		
            uart_puthex(ptr->dst >> 8);
            uart_puthex(ptr->dst & 0xff);
			if ( cache_entry_expired&(1<<i) )
			{
				uart_putch('-');
			}
			uart_putch('\r');
	
	}
}
