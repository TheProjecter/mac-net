/**********************************************************
 * file: queue.c
 * data: 2010-05-21
 * description: Operations for queue
 *********************************************************/
#include "queue.h"
#include "hal_uart.h"
#include "mac.h"

void queue_init(struct queue *que)
{
	que->queue_head = 0;
	que->queue_tail = 0;
	que->queue_full = FALSE;
}

BOOL queue_is_full(struct queue *que)
{
	return que->queue_full;
}

BOOL queue_is_empty(struct queue *que)
{
	return (que->queue_full == FALSE && que->queue_head == que->queue_tail);
}

// load data from UART buffer to queue
void queue_add_element(struct queue *que, UINT8 len, UINT8 *p_cksum)
{
	UINT8 i;
	UINT8 * p;
	UINT8 c;
	struct data_entry *temp = (que->queue_data + que->queue_head);

	temp->p_len = len;

	p = temp->data_arr;

	for (i = 0; i < len; i++) {
		c = uart_rdy_getch();
		*(p + i) = c;
		*(p_cksum) += c;		// update checksum
	}

	que->queue_head++;
	if ( que->queue_head >= QUEUE_LENGTH )
	{
		que->queue_head = 0;
	}

	// See if full after this element is added
	if ( que->queue_head == que->queue_tail && que->queue_full == FALSE )
	{
		que->queue_full = TRUE;
	}
}

struct data_entry *queue_get_next(struct queue *que)
{
	struct data_entry *temp = (que->queue_data + que->queue_tail);

	return temp;
}

void queue_remove_next(struct queue *que)
{
	que->queue_tail++;

	if ( que->queue_tail >= QUEUE_LENGTH )
	{
		que->queue_tail = 0;
	}

	// Clear the full indicator if it is true
	que->queue_full = FALSE;
}

/*
 * remove the head entry in the queue
 */
void queue_remove_head(struct queue *que)
{
	// Not empty
	if (queue_is_empty(que))
	{
		return;
	}

	// Mark the last enqueued data invalid.
	que->queue_head = (que->queue_head - 1 + QUEUE_LENGTH)%QUEUE_LENGTH;

	// Update state flag.
	que->queue_full = FALSE;
}
