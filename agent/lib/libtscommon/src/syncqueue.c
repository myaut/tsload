
/*
    This file is part of TSLoad.
    Copyright 2012-2014, Sergey Klyaus, ITMO University

    TSLoad is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation version 3.

    TSLoad is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TSLoad.  If not, see <http://www.gnu.org/licenses/>.    
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
	cv_init(&sq->sq_cv, "sq-%s", sq->sq_name);

	sq->sq_head = NULL;
	sq->sq_tail = NULL;

	sq->sq_is_destroyed = B_FALSE;
}

/**
 * Extract element from synchronized queue.
 * If no elements on queue, blocks until squeue_push will add an element.
 *
 * May be called from multiple threads simultaneously, thread selected
 * in cv_notify_one wins (undetermined for most platforms).
 *
 * @param sq queue
 *
 * @return element or NULL if squeue is destroyed
 */
void* squeue_pop(squeue_t* sq) {
	squeue_el_t* el = NULL;
	void *object = NULL;

	mutex_lock(&sq->sq_mutex);

retry:
	/* Try to extract element from head*/
	el = sq->sq_head;

	if(el == NULL) {
		cv_wait(&sq->sq_cv, &sq->sq_mutex);

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
 * Put an element on synchronized queue.
 *
 * @param object element
 */
void squeue_push(squeue_t* sq, void* object) {
	squeue_el_t* el = mp_malloc(sizeof(squeue_el_t));

	boolean_t notify = B_FALSE;

	el->s_next = NULL;
	el->s_data = object;

	mutex_lock(&sq->sq_mutex);

	/* */
	assert(!(sq->sq_head == NULL && sq->sq_tail != NULL));

	if(sq->sq_head == NULL) {
		/* Empty queue, notify pop*/
		sq->sq_head = el;

		notify = B_TRUE;
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

	if(notify)
		cv_notify_one(&sq->sq_cv);
}

/**
 * Destroy all elements in queue and queue itself. Also notifies squeue_pop
 * Doesn't deallocate squeue_t
 *
 * @param sq synchronized queue to destroy
 * @param free helper to destroy element's data
 *
 * @note squeue_destroy() will notify consumer but doesn't guarantee that it \
 * 		will leave squeue_pop(). You need to check this on your own.		 \
 * 		It could be easily done by joining consumer thread.
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
	cv_notify_all(&sq->sq_cv);

	mutex_unlock(&sq->sq_mutex);

	mutex_destroy(&sq->sq_mutex);
	cv_destroy(&sq->sq_cv);
}

