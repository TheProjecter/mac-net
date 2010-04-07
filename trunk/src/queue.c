/**********************************************************
 * file: queue.c
 * data: 2009.04.26
 * description: Operations for queue
 *********************************************************/
#include "queue.h"
#include "hal_uart.h"
#include "nwk.h"

void queue_init(struct queue *que)
{
	int i;
	struct data_entry *temp = que->queue_data;

	que->queue_head = 0;
	que->queue_tail = 0;
	que->queue_full = FALSE;

	for ( i = 0; i < QUEUE_LENGTH; i++ )
	{
		(temp + i)->dst_addr = 0;
		(temp + i)->req_time = 0;
	}
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
void queue_add_element(struct queue *que, UINT8 len, UINT16 dst_addr, UINT8 *p_cksum)
{
	UINT8 i;
	UINT8 * p;
	UINT8 c;
	struct data_entry *temp = (que->queue_data + que->queue_head);

	temp->dst_addr = dst_addr;
	temp->req_time = 0;
	temp->p_len = len;

	p = temp->data_arr;
	for (i = 0; i < sizeof(nwk_opt_t); i++)
		*(p+i) = 0;			// reset to zero

	p = temp->data_arr + sizeof (nwk_opt_t);
	for (i = 0; i < len; i++) {
		c = uart_getch();
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
	if ( que->queue_full )
	{
		que->queue_full = FALSE;
	}
}

/*
 * remove the head entry in the queue
 */
void queue_remove_head(struct queue *que)
{
	//TODO: sanity check
	
	struct data_entry *temp = (que->queue_data + que->queue_head);

	temp->dst_addr = 0;
	temp->req_time = 0;
	temp->p_len = 0;

	if (que->queue_head == 0)
		que->queue_head = QUEUE_LENGTH - 1;
	else
		que->queue_head --;
	
	que->queue_full = FALSE;		// of course queue is not full now

}
