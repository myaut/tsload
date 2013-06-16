/*
 * linux/cpumask.h
 *
 *  Created on: 15.06.2013
 *      Author: myaut
 */

#ifndef PLAT_CPUMASK_H_
#define PLAT_CPUMASK_H_

#include <defs.h>
#include <mempool.h>

#include <stdlib.h>

#include <sched.h>

#define MAX_CPUS		256

#define HAVE_CPUMASK_BASIC
#define HAVE_CPUMASK_COUNT
#define HAVE_CPUMASK_EQ

typedef struct cpumask {
	size_t    size;
	cpu_set_t set;
} cpumask_t;

STATIC_INLINE void cpumask_reset(cpumask_t* mask) {
	CPU_ZERO_S(mask->size, &mask->set);
}

STATIC_INLINE void cpumask_set(cpumask_t* mask, int cpuid) {
	CPU_SET_S(cpuid, mask->size, &mask->set);
}

STATIC_INLINE void cpumask_clear(cpumask_t* mask, int cpuid) {
	CPU_CLR_S(cpuid, mask->size, &mask->set);
}

STATIC_INLINE boolean_t cpumask_isset(cpumask_t* mask, int cpuid) {
	return CPU_ISSET_S(cpuid, mask->size, &mask->set);
}

STATIC_INLINE int cpumask_count(cpumask_t* mask) {
	return CPU_COUNT_S(mask->size, &mask->set);
}

STATIC_INLINE boolean_t cpumask_eq(cpumask_t* a, cpumask_t* b) {
	if(a->size != b->size)
		return B_FALSE;

	return CPU_EQUAL_S(a->size, &a->set, &b->set);
}

#endif /* PLAT_CPUMASK_H_ */
