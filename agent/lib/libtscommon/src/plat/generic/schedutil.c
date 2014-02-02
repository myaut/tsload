/*
 * schedutil.c
 *
 *  Created on: 16.06.2013
 *      Author: myaut
 */

#include <schedutil.h>
#include <threads.h>

PLATAPIDECL(sched_get_policies) sched_policy_t* sched_policies[1] = { NULL };


PLATAPI void plat_tsched_init(thread_t* thread) {

}

PLATAPI void plat_tsched_destroy(thread_t* thread) {

}

PLATAPI int sched_get_cpuid(void) {
	return SCHED_NOT_SUPPORTED;
}

PLATAPI int sched_set_affinity(thread_t* thread , cpumask_t* mask) {
	return SCHED_NOT_SUPPORTED;
}

PLATAPI int sched_get_affinity(thread_t* thread , cpumask_t* mask) {
	return SCHED_NOT_SUPPORTED;
}

PLATAPI int sched_switch(void) {
	return SCHED_OK;
}

PLATAPI sched_policy_t** sched_get_policies() {
	return sched_policies;
}

PLATAPI int sched_set_policy(thread_t* thread, const char* name) {
	return SCHED_INVALID_POLICY;
}

PLATAPI int sched_set_param(thread_t* thread, const char* name, int64_t value) {
	return SCHED_INVALID_PARAM;
}

PLATAPI int sched_commit(thread_t* thread) {
	return SCHED_NOT_SUPPORTED;
}

PLATAPI int sched_get_param(thread_t* thread, const char* name, int64_t* value) {
	return SCHED_INVALID_PARAM;
}

PLATAPI int sched_get_policy(thread_t* thread, char* name, size_t len) {
	return SCHED_NOT_SUPPORTED;
}

PLATAPI int sched_init(void) {
	return 0;
}

PLATAPI void sched_fini(void) {

}
