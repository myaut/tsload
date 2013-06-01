/*
 * syncqueue.c
 *
 *  Created on: 27.11.2012
 *      Author: myaut
 */

#include <defs.h>
#include <mempool.h>
#include <threads.h>
#include <syncqueue.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/**
 * Initialize synchronized queue
 *
 * @param sq - queue
 * @param name - name of queue (for debugging purposes)
 */
void squeue_init(squeue_t* sq, const char* namefmt, ...) {
	va_list va;

	va_start(va, namefmt);
	vsnprintf(sq->sq_name, SQUEUENAMELEN, namefmt, va);
	va_end(va);

	mutex_init(&sq->sq_mutex, "sq-%s", sq->sq_name);
	event_init(&sq->sq_event, "sq-%s", sq->sq_name);

	sq->sq_head = NULL;
	sq->sq_tail = NULL;

	sq->sq_is_destroyed = B_FALSE;
}

/**
 * Extract element from synchronized queue.
 * If no elements on queue, blocks until squeue_push will add an element.
 *
 * May be called from multiple threads simultainously, thread selected
 * in event_notify_one wins.
 *
 * @param sq queue
 *
 * @return element or NULL if squeue is destroyed
 */
void* squeue_pop(squeue_t* sq) {
	squeue_el_t* el = NULL;
	void *object = NULL;

retry:
	mutex_lock(&sq->sq_mutex);

	/* Try to extract element from head*/
	el = sq->sq_head;

	if(el == NULL) {
		event_wait_unlock(&sq->sq_event, &sq->sq_mutex);

		if(!sq->sq_is_destroyed)
			goto retry;
		else
			return NULL;
	}

	/* Move backward */
	sq->sq_head = el->s_next;

	/* Only one element left and it is head,
	 * clear tail pointer*/
	if(sq->sq_tail == sq->sq_head) {
		sq->sq_tail = NULL;
	}

	mutex_unlock(&sq->sq_mutex);

	object = el->s_data;
	mp_free(el);

	return object;
}

/**
 * Add an element to synchronized queue.
 *
 * @param object element
 */
void squeue_push(squeue_t* sq, void* object) {
	squeue_el_t* el = mp_malloc(sizeof(squeue_el_t));

	el->s_next = NULL;
	el->s_data = object;

	mutex_lock(&sq->sq_mutex);

	/* */
	assert(!(sq->sq_head == NULL && sq->sq_tail != NULL));

	if(sq->sq_head == NULL) {
		/* Empty queue, notify pop*/
		sq->sq_head = el;

		event_notify_one(&sq->sq_event);
	}
	else if(sq->sq_tail == NULL) {
		/* Only sq_head present, insert el as tail and link head with tail */
		sq->sq_tail = el;
		sq->sq_head->s_next = el;
	}
	else {
		/* Add element and move tail forward */
		sq->sq_tail->s_next = el;
		sq->sq_tail = el;
	}

	mutex_unlock(&sq->sq_mutex);
}

/*
 * Destroy all elements in queue and queue itself
 * Also notifies squeue_pop
 *
 * @param sq synchronized queue to destroy
 * @param free helper to destroy element's data
 * */
void squeue_destroy(squeue_t* sq, void (*el_free)(void* obj)) {
	squeue_el_t* el;
	squeue_el_t* next;

	mutex_lock(&sq->sq_mutex);

	el = sq->sq_head;

	while(el != sq->sq_tail) {
		next = el->s_next;

		el_free(el->s_data);
		mp_free(el);

		el = next;
	}

	sq->sq_is_destroyed = B_TRUE;
	event_notify_all(&sq->sq_event);

	mutex_unlock(&sq->sq_mutex);

	mutex_destroy(&sq->sq_mutex);
}
