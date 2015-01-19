
/*
    This file is part of TSLoad.
    Copyright 2013-2014, Sergey Klyaus, ITMO University

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

#include <tsload/mempool.h>
#include <tsload/threads.h>
#include <tsload/schedutil.h>

#include <hostinfo/cpuinfo.h>

#include <assert.h>


#define NUM_THREADS 	2
#define NUM_CYCLES		20000
#define NUM_ITERATIONS  100

/* Tests Solaris processor sets
 *
 * Tries to bind thread to any processor at system except for one */

cpumask_t* mask = NULL;

void create_mask(void);

thread_result_t test_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, void, unused);

	tm_sleep_milli(100 * T_MS);

THREAD_END:
	THREAD_FINISH(arg);
}

int test_main() {
	int ret;

	thread_t thread;

	cpumask_t* thread_mask = NULL;

	mempool_init();
	threads_init();

	/* Create mask that contains all CPUs */
	mask = cpumask_create();
	create_mask();

	/* Initialize thread */
	t_init(&thread, NULL, test_thread, "thread");
	t_wait_start(&thread);

	ret = sched_set_affinity(&thread, mask);

	if(ret != SCHED_NOT_SUPPORTED) {
		assert(ret == SCHED_OK);

		/* Check if thread have correct affinity */
		thread_mask = cpumask_create();
		cpumask_reset(thread_mask);
		ret = sched_get_affinity(&thread, thread_mask);

		assert(cpumask_eq(mask, thread_mask));

		cpumask_destroy(thread_mask);
	}

	/* Clean up the mess */
	t_destroy(&thread);

	cpumask_destroy(mask);

	threads_fini();
	mempool_fini();

	return 0;
}

void create_mask(void) {
	list_head_t* cpu_list;
	hi_object_t* object;
	hi_cpu_object_t* cpuobj;

	boolean_t first_cpu = B_TRUE;

	hi_obj_init();
	cpu_list = hi_cpu_list(B_FALSE);

	hi_for_each_object(object, cpu_list) {
		cpuobj = HI_CPU_FROM_OBJ(object);

		if(cpuobj->type == HI_CPU_STRAND) {
			if(!first_cpu)
				cpumask_set(mask, cpuobj->id);
			else
				first_cpu = B_FALSE;
		}
	}

	hi_obj_fini();
}

