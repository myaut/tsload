/*
 * syncqueue.h
 *
 *  Created on: 27.11.2012
 *      Author: myaut
 */

#ifndef SYNCQUEUE_H_
#define SYNCQUEUE_H_

#include <threads.h>

#define SQUEUENAMELEN		24

typedef struct squeue_el {
	struct squeue_el* s_next;

	void* s_data;
} squeue_el_t;

typedef struct squeue {
	thread_mutex_t sq_mutex;
	thread_cv_t    sq_cv;

	squeue_el_t* sq_head;
	squeue_el_t* sq_tail;

	char sq_name[SQUEUENAMELEN];

	boolean_t sq_is_destroyed;
} squeue_t;

LIBEXPORT void squeue_init(squeue_t* sq, const char* namefmt, ...);
LIBEXPORT void squeue_push(squeue_t* sq, void* object);
LIBEXPORT void* squeue_pop(squeue_t* sq);
LIBEXPORT void squeue_destroy(squeue_t* sq, void (*el_free)(void* obj));

#endif /* SYNCQUEUE_H_ */
