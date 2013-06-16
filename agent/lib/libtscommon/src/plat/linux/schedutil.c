/*
 * schedutil.c
 *
 *  Created on: 16.06.2013
 *      Author: myaut
 */

#include <schedutil.h>

#include <errno.h>
#include <pthread.h>

PLATAPI int sched_get_cpuid(void) {
	int cpuid = sched_getcpu();

	if(cpuid == -1) {
		if(errno == ENOSYS)
			return SCHED_NOT_SUPPORTED;
		return SCHED_ERROR;
	}

	return cpuid;
}

PLATAPI int sched_set_affinity(thread_t* t , cpumask_t* mask) {
	int ret = pthread_setaffinity_np(t->t_impl.t_thread,
			mask->size, &mask->set);

	if(ret != 0)
		return SCHED_ERROR;

	return SCHED_OK;
}

PLATAPI int sched_get_affinity(thread_t* t , cpumask_t* mask) {
	int ret = pthread_getaffinity_np(t->t_impl.t_thread,
				mask->size, &mask->set);

	if(ret != 0)
		return SCHED_ERROR;

	return SCHED_OK;
}

PLATAPI int sched_switch(void) {
	int ret = sched_yield();

	if(ret != 0)
		return SCHED_ERROR;

	return SCHED_OK;
}
