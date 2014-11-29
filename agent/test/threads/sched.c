
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
thread_event_t ev;

thread_result_t test_thread_sched(thread_arg_t arg) {
	THREAD_ENTRY(arg, void, unused);

	event_notify_one(&ev);
	event_wait(&ev);

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
	int value;
	int pid;
	int pcount = 0;

	threads_init();
	sched_init();

	event_init(&ev, "test_thread_start");
	t_init(&t, NULL, test_thread_sched, "test_thread_sched");

	event_wait(&ev);

	assert(sched_set_policy(&t, policy) == SCHED_OK);

#ifndef NO_PARAMS
	pcount = sizeof(params) / sizeof(struct param);

	for(pid = 0; pid < pcount; ++pid) {
		assert(sched_set_param(&t, params[pid].name, params[pid].value) == SCHED_OK);
	}
#endif

	assert(sched_commit(&t) == SCHED_OK);

	assert(sched_get_policy(&t, policy_value, 10) == SCHED_OK);
	assert(strcmp(policy, policy_value) == 0);

#ifndef NO_PARAMS
	for(pid = 0; pid < pcount; ++pid) {
		assert(sched_get_param(&t, params[pid].name, &value) == SCHED_OK);
		assert(value == params[pid].value);
	}
#endif

	event_notify_one(&ev);

	t_join(&t);
	t_destroy(&t);

	event_destroy(&ev);

	sched_fini();
	threads_fini();

	return 0;
}

