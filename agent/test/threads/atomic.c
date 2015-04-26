
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

#include <tsload/atomic.h>
#include <tsload/threads.h>

#include <assert.h>


#define NUM_THREADS		4
#define STEPS			4096

/* Atomically increased counter */
atomic_t counter = 0;

thread_result_t test_mutex(thread_arg_t arg) {
	THREAD_ENTRY(arg, void, unused);
	int step = 0;

	while(step++ < STEPS) {
		atomic_inc(&counter);
	}

THREAD_END:
	THREAD_FINISH(arg);
}

int test_main() {
	thread_t threads[NUM_THREADS];
	int tid;

	threads_init();

	for(tid = 0; tid < NUM_THREADS; ++tid) {
		t_init(&threads[tid], NULL, test_mutex,
					"tatomic-%d", tid);
	}

	for(tid = 0; tid < NUM_THREADS; ++tid) {
		t_join(&threads[tid]);
		t_destroy(&threads[tid]);
	}

	assert(atomic_read(&counter) == (NUM_THREADS * STEPS));

	threads_fini();

	return 0;
}

