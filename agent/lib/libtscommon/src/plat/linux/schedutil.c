/*
 * schedutil.c
 *
 *  Created on: 16.06.2013
 *      Author: myaut
 */

#include <schedutil.h>

#include <errno.h>
#include <pthread.h>
#include <string.h>

#include <sched.h>
#include <sys/time.h>
#include <sys/resource.h>

#define NICE_MIN				-20
#define NICE_MAX				19
#define NICE_NOT_SET			-100


#define DECLARE_LINUX_SCHED_NICE(name, id)				\
	sched_param_t sched_ ## name ## _params[] = {		\
		DECLARE_SCHED_PARAM_STATIC("nice", 				\
				NICE_MIN, NICE_MAX),					\
		DECLARE_SCHED_PARAM_END()						\
	};													\
	DECLARE_SCHED_POLICY(								\
		name, sched_ ## name ## _params, id);

DECLARE_LINUX_SCHED_NICE(normal, SCHED_OTHER);
DECLARE_LINUX_SCHED_NICE(batch, SCHED_BATCH);
DECLARE_LINUX_SCHED_NICE(idle, SCHED_IDLE);

sched_param_t sched_rr_params[] = {
	DECLARE_SCHED_PARAM("priority"),
	DECLARE_SCHED_PARAM_GET("interval"),
	DECLARE_SCHED_PARAM_END()
};
DECLARE_SCHED_POLICY(rr, sched_rr_params, SCHED_RR);

sched_param_t sched_fifo_params[] = {
	DECLARE_SCHED_PARAM("priority"),
	DECLARE_SCHED_PARAM_END()
};
DECLARE_SCHED_POLICY(fifo, sched_fifo_params, SCHED_FIFO);

PLATAPIDECL(sched_get_policies) sched_policy_t* sched_policies[6] = {
	&sched_normal_policy, &sched_batch_policy, &sched_idle_policy,
	&sched_rr_policy, &sched_fifo_policy, NULL };

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

PLATAPI sched_policy_t** sched_get_policies() {
	return sched_policies;
}

PLATAPI void plat_tsched_init(thread_t* thread) {
	thread->t_sched_impl.scheduler = -1;
	thread->t_sched_impl.param.sched_priority = 0;
	thread->t_sched_impl.nice = NICE_NOT_SET;
}

static void sched_initialize_rt(sched_policy_t* policy) {
	sched_param_t* priority;
	sched_param_t* interval;

	priority = &policy->params[0];

	priority->min = sched_get_priority_min(policy->id);
	priority->max = sched_get_priority_max(policy->id);
}

PLATAPI int sched_init() {
	/* FIXME: Not all policies may be supported by particular Linux Kernel */

	sched_initialize_rt(&sched_rr_policy);
	sched_initialize_rt(&sched_fifo_policy);

	return SCHED_OK;
}

PLATAPI int sched_set_policy(thread_t* thread, const char* name) {
	sched_policy_t* policy = sched_policy_find_byname(name);

	if(policy == NULL)
		return SCHED_INVALID_POLICY;

	thread->t_sched_impl.scheduler = policy->id;
	return SCHED_OK;
}

PLATAPI int sched_set_param(thread_t* thread, const char* name, int64_t value) {
	sched_policy_t* policy;
	sched_param_t* priority;

	int scheduler = thread->t_sched_impl.scheduler;

	if(scheduler == -1) {
		return SCHED_INVALID_POLICY;
	}

	if(scheduler == SCHED_RR || scheduler == SCHED_FIFO) {
		if(strcmp(name, "priority") == 0) {
			policy = sched_policy_find_byid(scheduler);
			priority = &policy->params[0];

			if(value < priority->min || value > priority->max)
				return SCHED_INVALID_VALUE;

			thread->t_sched_impl.param.sched_priority = value;

			return SCHED_OK;
		}
	}
	else {
		if(strcmp(name, "nice") == 0) {
			if(value < NICE_MIN || value > NICE_MAX)
				return SCHED_INVALID_VALUE;

			thread->t_sched_impl.nice = value;
			return SCHED_OK;
		}
	}

	return SCHED_INVALID_PARAM;
}

PLATAPI int sched_commit(thread_t* thread) {
	int err;

	int scheduler = thread->t_sched_impl.scheduler;
	int priority = thread->t_sched_impl.param.sched_priority;
	int nice = thread->t_sched_impl.nice;

	if(scheduler == -1) {
		return SCHED_INVALID_POLICY;
	}

	if(scheduler == SCHED_RR && scheduler == SCHED_FIFO) {
		if(priority == -1) {
			return SCHED_INVALID_PARAM;
		}
	}
	else {
		if(nice == NICE_NOT_SET) {
			return SCHED_INVALID_PARAM;
		}
	}

	err = pthread_setschedparam(thread->t_impl.t_thread,
								thread->t_sched_impl.scheduler,
								&thread->t_sched_impl.param);
	if(err != 0)
		return SCHED_ERROR;

	if(scheduler != SCHED_RR && scheduler != SCHED_FIFO) {
#ifdef HAVE_DECL___NR_GETTID
		err = setpriority(PRIO_PROCESS, thread->t_system_id, nice);

		if(err != 0)
			return SCHED_ERROR;
#else
		return SCHED_NOT_SUPPORTED;
#endif
	}

	return SCHED_OK;
}

PLATAPI int sched_get_param(thread_t* thread, const char* name, int64_t* value) {
	int scheduler;
	struct sched_param param;
	int err;
	int nice;

	err = pthread_getschedparam(thread->t_impl.t_thread, &scheduler, &param);

	if(err != 0)
		return SCHED_ERROR;

	if(strcmp(name, "priority") == 0) {
		if(scheduler != SCHED_RR && scheduler != SCHED_FIFO)
			return SCHED_INVALID_PARAM;

		*value = param.sched_priority;
		return SCHED_OK;
	}
	else if(strcmp(name, "inteval") == 0) {
		struct timespec ts;

		if(scheduler != SCHED_RR)
			return SCHED_INVALID_PARAM;

#ifdef HAVE_DECL___NR_GETTID
		err = sched_rr_get_interval(thread->t_system_id, &ts);

		if(err < 0)
			return SCHED_ERROR;

		*value = ts.tv_sec * T_SEC + ts.tv_nsec;
		return SCHED_OK;
#else
		return SCHED_NOT_SUPPORTED;
#endif
	}
	else if(strcmp(name, "nice") == 0) {
#ifdef HAVE_DECL___NR_GETTID
		errno = 0;
		nice = getpriority(PRIO_PROCESS, thread->t_system_id);

		if(errno != 0)
			return SCHED_ERROR;

		*value = nice;
		return SCHED_OK;
#else
		return SCHED_NOT_SUPPORTED;
#endif
	}

	return SCHED_INVALID_PARAM;
}

PLATAPI int sched_get_policy(thread_t* thread, char* name, size_t len) {
	int scheduler;
	struct sched_param param;
	int err = pthread_getschedparam(thread->t_impl.t_thread, &scheduler, &param);
	sched_policy_t* policy;

	if(err != 0)
		return SCHED_ERROR;

	policy = sched_policy_find_byid(scheduler);

	if(policy == NULL)
		return SCHED_ERROR;

	strncpy(name, policy->name, len);
	return SCHED_OK;
}

PLATAPI void sched_fini() {

}
