/*
 * mutex.c
 *
 *  Created on: Apr 25, 2013
 *      Author: myaut
 */


#include <threads.h>
#include <atomic.h>

#include <assert.h>

#define NUM_THREADS		16
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
