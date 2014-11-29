
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

#include <tsload/schedutil.h>
#include <tsload/threads.h>

#include <windows.h>


#define DECLARE_WIN_SCHED_POLICY(name, id)				\
	sched_param_t sched_ ## name ## _params[] = {		\
		DECLARE_SCHED_PARAM_END()						\
	};													\
	DECLARE_SCHED_POLICY(name,sched_ ## name ## _params, id)

DECLARE_WIN_SCHED_POLICY(idle, THREAD_PRIORITY_IDLE);
DECLARE_WIN_SCHED_POLICY(lowest, THREAD_PRIORITY_LOWEST);
DECLARE_WIN_SCHED_POLICY(below_normal, THREAD_PRIORITY_BELOW_NORMAL);
DECLARE_WIN_SCHED_POLICY(normal, THREAD_PRIORITY_NORMAL);
DECLARE_WIN_SCHED_POLICY(above_normal, THREAD_PRIORITY_ABOVE_NORMAL);
DECLARE_WIN_SCHED_POLICY(highest, THREAD_PRIORITY_HIGHEST);
DECLARE_WIN_SCHED_POLICY(time_critical, THREAD_PRIORITY_TIME_CRITICAL);

PLATAPIDECL(sched_get_policies) sched_policy_t* sched_policies[8] = {
	&sched_idle_policy, &sched_lowest_policy, &sched_below_normal_policy,
	&sched_normal_policy, &sched_above_normal_policy, &sched_highest_policy,
	&sched_time_critical_policy, NULL
};

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

PLATAPI sched_policy_t** sched_get_policies() {
	return sched_policies;
}

PLATAPI void plat_tsched_init(thread_t* thread) {
	thread->t_sched_impl.priority = THREAD_PRIORITY_NORMAL;
}


PLATAPI int sched_set_policy(thread_t* thread, const char* name) {
	sched_policy_t* policy = sched_policy_find_byname(name);

	if(policy == NULL)
		return SCHED_INVALID_POLICY;

	thread->t_sched_impl.priority = policy->id;
	return SCHED_OK;
}

PLATAPI int sched_commit(thread_t* thread) {
	if(SetThreadPriority(thread->t_impl.t_handle, thread->t_sched_impl.priority) == 0)
		return SCHED_ERROR;

	return SCHED_OK;
}

PLATAPI int sched_get_policy(thread_t* thread, char* name, size_t len) {
	int priority = GetThreadPriority(thread->t_impl.t_handle);
	sched_policy_t* policy;

	if(priority == THREAD_PRIORITY_ERROR_RETURN)
		return SCHED_ERROR;

	policy = sched_policy_find_byid(priority);

	if(policy == NULL)
		return SCHED_ERROR;

	strncpy(name, policy->name, len);
	return SCHED_OK;
}

