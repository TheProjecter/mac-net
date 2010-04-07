#ifndef DSR_H
#define DSR_H


/* option types */
#define ROUTE_REQ   0
#define ROUTE_REPLY 1
#define ROUTE_ERROR 2
#define ROUTE_DATA  3
#define ROUTE_NONE  4


#define REQ_WATIT_TIME 20		// x ms timeout to wait reply


/* ietf draft definitions */
#define MAX_ROUTE_LEN        4   /* nodes default 15 */
/* own definitions */
#define ROUTE_CACHE_SIZE 8       /* entries */
#endif
