
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

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

#include <tsload/time.h>
#include <tsload/threads.h>
#include <tsload/schedutil.h>

#include <string.h>
#include <assert.h>


thread_t t;

boolean_t finished = B_FALSE;
thread_mutex_t mtx;
thread_cv_t cv;

thread_result_t test_thread_sched(thread_arg_t arg) {
	THREAD_ENTRY(arg, void, unused);
	
	mutex_lock(&mtx);
	if(!finished)
		cv_wait(&cv, &mtx);
	mutex_unlock(&mtx);

THREAD_END:
	THREAD_FINISH(arg);
}

struct param {
	const char* name;
	int value;
};

#if defined(PLAT_LINUX)
const char*	policy = "idle";
struct param params[] = {{"nice", 10}};
#elif defined(PLAT_WIN)
const char*	policy = "idle";
#define NO_PARAMS
#elif defined(PLAT_SOLARIS)
const char*	policy = "FX";
struct param params[] = {{"uprilim", 20}, {"upri", 10}};
#endif

int test_main() {
	char policy_value[10];
	int pcount = 0;
#ifndef NO_PARAMS    
    int64_t value;
    int pid;
#endif
	
	int ret;
	
	threads_init();
	sched_init();

    mutex_init(&mtx, "test_thread_mtx");
	cv_init(&cv, "test_thread_end");
	t_init(&t, NULL, test_thread_sched, "test_thread_sched");

	t_wait_start(&t);

	assert(sched_set_policy(&t, policy) == SCHED_OK);

#ifndef NO_PARAMS
	pcount = sizeof(params) / sizeof(struct param);

	for(pid = 0; pid < pcount; ++pid) {
		assert(sched_set_param(&t, params[pid].name, params[pid].value) == SCHED_OK);
	}
#endif
	
	ret = sched_commit(&t);
	assert(ret == SCHED_OK || ret == SCHED_NOT_PERMITTED);
	
	if(ret == SCHED_OK) {
		assert(sched_get_policy(&t, policy_value, 10) == SCHED_OK);
		assert(strcmp(policy, policy_value) == 0);

#ifndef NO_PARAMS
		for(pid = 0; pid < pcount; ++pid) {
			assert(sched_get_param(&t, params[pid].name, &value) == SCHED_OK);
			assert(value == params[pid].value);
		}
#endif
	}
	
	mutex_lock(&mtx);
	finished = B_TRUE;
	cv_notify_one(&cv);
	mutex_unlock(&mtx);

	t_join(&t);
	t_destroy(&t);

	cv_destroy(&cv);

	sched_fini();
	threads_fini();

	return 0;
}

