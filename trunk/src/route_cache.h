/*
 * file: route_cache.h
 * date: 2009-11-04
 * description: route cache management
 */

#ifndef ROUTE_CACHE_H
#define ROUTE_CACHE_H
#include "types.h"
#include "nwk.h"
#include "dsr.h"

/* route cache structures */
struct rt_entry {
  UINT8		segs_left;
  UINT16	addr[MAX_ROUTE_LEN];
  UINT16	dst;
};

extern UINT8 cache_entry_expired;
 

void rt_cache_init();
UINT8 rt_lookup_cache_index(UINT16 taddr);
struct rt_entry* rt_lookup_route(UINT16 dst);
void rt_update_forward_cache(nwk_opt_t * opt);
void rt_update_reverse_cache(nwk_opt_t * opt);
void rt_dump();


#endif
