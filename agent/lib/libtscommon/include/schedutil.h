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
 * schedutil - varios routineds that work with OS scheduler
 */

#define SCHED_OK				0
#define SCHED_ERROR				-1

#define SCHED_NOT_SUPPORTED		-2

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

#endif /* SCHEDUTIL_H_ */
