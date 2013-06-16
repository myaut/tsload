/*
 * affinity.c
 *
 *  Created on: 16.06.2013
 *      Author: myaut
 */

#include <threads.h>
#include <schedutil.h>
#include <mempool.h>

#include <cpuinfo.h>

#include <assert.h>

#define NUM_THREADS 	2
#define NUM_CYCLES		20000
#define NUM_ITERATIONS  100

/* Tests thread affinity.
 *
 * Creates two cpu-intensive threads that run concurrently on
 * same CPU. If affinity works, they will never migrate to another CPU. */

int cpuid = 0;
cpumask_t* mask = NULL;

int get_first_cpu(void);

thread_result_t test_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, void, unused);
	volatile int i, j;

	sched_set_affinity(thread, mask);

	tm_sleep_milli(10 * T_MS);

	for(j = 0; j < NUM_ITERATIONS; ++j) {
		for(i = 0; i < NUM_CYCLES; ++i);
		sched_switch();	/* Wake up, scheduler */

		assert(sched_get_cpuid() == cpuid);
	}

THREAD_END:
	THREAD_FINISH(arg);
}

int test_main() {
	int ret;
	int i = 0;
	thread_t threads[NUM_THREADS];

	cpumask_t* thread_mask = NULL;

	mempool_init();
	threads_init();

	/* Get cpu id and mask for it */
	cpuid = get_first_cpu();
	mask = cpumask_create();
	cpumask_set(mask, cpuid);

	for(i = 0; i < NUM_THREADS; ++i) {
		t_init(threads + i, NULL,
			   test_thread, "thread-%d", i);
	}

	tm_sleep_milli(10 * T_MS);

	thread_mask = cpumask_create();
	for(i = 0; i < NUM_THREADS; ++i) {
		/* Threads should have correct affinity! */
		cpumask_reset(thread_mask);
		ret = sched_get_affinity(threads + i, thread_mask);

		if(ret != SCHED_NOT_SUPPORTED) {
			assert(cpumask_eq(mask, thread_mask));
		}
	}

	for(i = 0; i < NUM_THREADS; ++i) {
		t_destroy(threads + i);
	}

	cpumask_destroy(thread_mask);
	cpumask_destroy(mask);

	threads_fini();
	mempool_fini();

	return 0;
}

int get_first_cpu(void) {
	list_head_t* cpu_list;
	hi_object_t* object;
	hi_cpu_object_t* cpuobj;

	int cpuid = -1;

	hi_obj_init();
	cpu_list = hi_cpu_list(B_FALSE);

	hi_for_each_object(object, cpu_list) {
		cpuobj = HI_CPU_FROM_OBJ(object);

		if(cpuobj->type == HI_CPU_STRAND) {
			cpuid = cpuobj->id;
			break;
		}
	}

	hi_obj_fini();
	return cpuid;
}
