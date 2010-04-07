/*
 * file nwk.h
 * date: 2009-11-04
 * description: nwk header
 */

#ifndef NWK_H
#define NWK_H
#include "types.h"
#include "dsr.h"

typedef struct _nwk_opt{
  UINT8  type;		// route type
  union {
  	UINT8 ident;	// used as id when request
	UINT8 hops;		// hops in the route
  };
  UINT16 taddr;		// target address
  union {
  UINT8  cur_len;	// current len to the source node
  UINT8  bcast_id;		// used as broadcast id
  };
  UINT16 addr[MAX_ROUTE_LEN];	// path
}nwk_opt_t;



void nwk_init();
void nwk_main();


extern struct queue nwk_send_queue;	// used by trans layer	

#endif
