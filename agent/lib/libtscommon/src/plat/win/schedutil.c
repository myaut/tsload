/*
 * schedutil.c
 *
 *  Created on: 16.06.2013
 *      Author: myaut
 */

#include <schedutil.h>

#include <windows.h>

PLATAPI int sched_get_cpuid(void) {
	/* TODO: Should support processor groups through GetCurrentProcessorNumberEx */
	return GetCurrentProcessorNumber();
}

PLATAPI int sched_set_affinity(thread_t* thread , cpumask_t* mask) {
	int pgid;
	uint64_t gmask;
	int gcount = 0;

	DWORD_PTR pmask;

	for(pgid = 0; pgid < MAX_CPU_GROUPS; ++pgid) {
		if(mask->groups[pgid] != 0ull) {
			gmask = mask->groups[pgid];
			gcount++;
		}
	}

	/* Mask from different processor groups, is not supported */
	if(gcount > 1)
		return SCHED_NOT_SUPPORTED;

	/* On 32-bit builds, only 32 processors per group are supported */
	if(sizeof(DWORD_PTR) == 4 && ((gmask >> 32) != 0))
		return SCHED_NOT_SUPPORTED;

	pmask = (DWORD_PTR) gmask;

	if(SetThreadAffinityMask(thread->t_impl.t_handle, pmask) == 0)
		return SCHED_ERROR;

	return SCHED_OK;
}

PLATAPI int sched_get_affinity(thread_t* thread , cpumask_t* mask) {
	/* TODO: Implement via NtQueryInformationThread or Processor groups
	 *
	 * See: http://stackoverflow.com/questions/6601862/query-thread-not-process-processor-affinity */
	return SCHED_NOT_SUPPORTED;
}

PLATAPI int sched_switch(void) {
	SwitchToThread();

	return SCHED_OK;
}

