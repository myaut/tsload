/*
 * linux_sched.c
 *
 *  Created on: Jan 28, 2014
 *      Author: myaut
 */


#include <threads.h>
#include <schedutil.h>
#include <tstime.h>

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
