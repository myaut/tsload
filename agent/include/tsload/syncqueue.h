
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



#ifndef SYNCQUEUE_H_
#define SYNCQUEUE_H_

#include <tsload/defs.h>


#define SQUEUENAMELEN		24

/**
 * @module Synchronized queue
 *
 * This queue may be used for implementation of producer-consumer models.
 * While producer creates objects and puts them onto queue via squeue_push(),
 * consumer (one or many) waits for it objects in squeue_pop() and awakes
 * if object was put on queue.
 *
 * Since there is no way to interrupt consumers, you may put NULL onto
 * queue and handle this situation in consumer code or call squeue_destroy()
 */

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

LIBEXPORT void squeue_init(squeue_t* sq, const char* namefmt, ...)
	CHECKFORMAT(printf, 2, 3);
LIBEXPORT void squeue_push(squeue_t* sq, void* object);
LIBEXPORT void* squeue_pop(squeue_t* sq);
LIBEXPORT void squeue_destroy(squeue_t* sq, void (*el_free)(void* obj));

#endif /* SYNCQUEUE_H_ */

