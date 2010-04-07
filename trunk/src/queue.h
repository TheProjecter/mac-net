/**********************************************************
 * file: queue.h
 * data: 2009.04.26
 * description: structure for queue
 *********************************************************/

#ifndef QUEUE_H
#define QUEUE_H
#include "types.h"

#define QUEUE_ENTRY_LENGTH 116		// 125 - 9
#define QUEUE_LENGTH 5

struct data_entry {
	UINT16 dst_addr;
	UINT32 req_time;			// the time when a request sent for this packet
	UINT8 p_len;			// The length of the data array, excluding nwk header
	UINT8 data_arr[QUEUE_ENTRY_LENGTH];	// The array that contains wnk header and payload
};

struct queue {
	struct data_entry queue_data[QUEUE_LENGTH];	// The data of current queue

	UINT8 queue_head;				// The head of current queue
	UINT8 queue_tail;				// The tail of current queue
	BOOL queue_full;				// Indicate if current queue is full?
};

// Initialize the queue
void queue_init(struct queue *que);
// If current queue is full
BOOL queue_is_full(struct queue *que);
// If current queue is empty
BOOL queue_is_empty(struct queue *que);
// Add element to queue. We assume queue_is_full is invoked before
void queue_add_element(struct queue *que, UINT8 len, UINT16 dst_addr, UINT8 *p_cksum);
// Get next element in current queue, don't remove. We assume queue_is_empty is invoked before
struct data_entry *queue_get_next(struct queue *que);
// Romve next element in current queue, We assume queue_is_empty is called before
void queue_remove_next(struct queue *que);
// remove from the head, used when the new inserted entry is invalid
void queue_remove_head(struct queue *que);

#endif /* QUEUE_H */

