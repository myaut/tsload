
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

