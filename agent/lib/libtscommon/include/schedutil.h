/*
 * schedutil.h
 *
 *  Created on: 16.06.2013
 *      Author: myaut
 */

#ifndef SCHEDUTIL_H_
#define SCHEDUTIL_H_

#include <defs.h>
#include <threads.h>
#include <cpumask.h>

/**
 * schedutil - various routines that work with OS scheduler
 */

#define SCHED_OK				0
#define SCHED_ERROR				-1
#define SCHED_NOT_SUPPORTED		-2
#define SCHED_INVALID_POLICY	-3
#define SCHED_INVALID_PARAM		-4
#define SCHED_INVALID_VALUE		-5

PLATAPI void plat_tsched_init(thread_t* thread);
PLATAPI void plat_tsched_destroy(thread_t* thread);

/**
 * Returns cpuid of current thread*/
LIBEXPORT PLATAPI int sched_get_cpuid(void);

/**
 * Yield CPU to another thread
 */
LIBEXPORT PLATAPI int sched_switch(void);

LIBEXPORT PLATAPI int sched_set_affinity(thread_t* thread , cpumask_t* mask);
LIBEXPORT PLATAPI int sched_get_affinity(thread_t* thread , cpumask_t* mask);

#define DECLARE_SCHED_PARAM_STATIC(name, min, max)		{name, min, max}
#define DECLARE_SCHED_PARAM(name)						{name, 0, 0}
#define DECLARE_SCHED_PARAM_GET(name)					{name, -1, -1}
#define DECLARE_SCHED_PARAM_END()						{NULL, 0, 0}

#define DECLARE_SCHED_POLICY(name, params, id)	\
	sched_policy_t sched_ ## name ## _policy = {		\
		#name, params, id	    						\
	};

typedef struct {
	const char* name;
	int 		min;
	int 		max;
} sched_param_t;

typedef struct {
	const char* name;
	sched_param_t* params;

	int   id;
	void* private;
} sched_policy_t;

sched_policy_t* sched_policy_find_byname(const char* name);
sched_policy_t* sched_policy_find_byid(int id);

/**
 * Returns array of available scheduler policies on current system
 * NULL is a mark for end of array
 */
LIBEXPORT PLATAPI sched_policy_t** sched_get_policies();

/**
 * Sets policy of a thread, but doesn't call operating system until you
 * call sched_commit. This is because some OSes (like Linux) need to set
 * policy and param simultaneously, and others (like Solaris) may have multiple
 * params to set.
 *
 * @return SCHED_INVALID_POLICY if name is invalid or SCHED_OK if everything went fine.
 */
LIBEXPORT PLATAPI int sched_set_policy(thread_t* thread, const char* name);

/**
 * Sets scheduler parameter
 *
 * @note if this function fails, you should start again from sched_set_policy
 *
 * @return SCHED_INVALID_PARAM if name is invalid, SCHED_INVALID_POLICY if policy is \
 *   not yet set and SCHED_OK if everything went fine.
 */
LIBEXPORT PLATAPI int sched_set_param(thread_t* thread, const char* name, int64_t value);

/**
 * Calls operating system to commit changes made by sched_set_policy() and sched_set_param()
 *
 * @return SCHED_ERROR in case of system error, SCHED_INVALID_PARAM/SCHED_INVALID_POLICY if \
 *     thread was not properly initialized
 */
LIBEXPORT PLATAPI int sched_commit(thread_t* thread);

LIBEXPORT PLATAPI int sched_get_param(thread_t* thread, const char* name, int64_t* value);
LIBEXPORT PLATAPI int sched_get_policy(thread_t* thread, char* name, size_t len);

LIBEXPORT PLATAPI int sched_init(void);
LIBEXPORT PLATAPI void sched_fini(void);

#endif /* SCHEDUTIL_H_ */
