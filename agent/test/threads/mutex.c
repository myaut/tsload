/*
 * mutex.c
 *
 *  Created on: Apr 25, 2013
 *      Author: myaut
 */


#include <threads.h>
#include <tstime.h>

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
