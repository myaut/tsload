
/*
    This file is part of TSLoad.
    Copyright 2013, Sergey Klyaus, ITMO University

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




#include <tsload/defs.h>

#include <tsload/time.h>
#include <tsload/threads.h>

#include <assert.h>


#define NUM_THREADS		16
#define STEPS			60

/* This variable will be concurrently accessed by threads.
 * If sem > 1 it means that mutex was failed*/
int sem = 0;

thread_mutex_t mtx;

thread_result_t test_mutex(thread_arg_t arg) {
	THREAD_ENTRY(arg, void, unused);
	int step = 0;

	while(step++ < STEPS) {
		mutex_lock(&mtx);

		assert(++sem == 1);
		--sem;

		mutex_unlock(&mtx);
	}

THREAD_END:
	THREAD_FINISH(arg);
}


int test_main() {
	thread_t threads[NUM_THREADS];
	int tid;

	threads_init();

	mutex_init(&mtx, "mutex");
	for(tid = 0; tid < NUM_THREADS; ++tid) {
		t_init(&threads[tid], NULL, test_mutex,
					"tmutex-%d", tid);
	}

	for(tid = 0; tid < NUM_THREADS; ++tid) {
		t_join(&threads[tid]);
		t_destroy(&threads[tid]);
	}

	mutex_destroy(&mtx);
	threads_fini();

	return 0;
}

