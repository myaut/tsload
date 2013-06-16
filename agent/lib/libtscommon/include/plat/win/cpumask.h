/*
 * cpumask.h
 *
 *  Created on: 15.06.2013
 *      Author: myaut
 */

#ifndef PLAT_CPUMASK_H_
#define PLAT_CPUMASK_H_

#include <defs.h>

#include <string.h>
#include <assert.h>

#define MAX_CPUS		256
#define MAX_CPU_GROUPS  (MAX_CPUS / 64)

#define HAVE_CPUMASK_BASIC

/**
 * see also - GROUP_AFFINITY
 *  */
typedef struct cpumask {
	uint64_t groups[MAX_CPU_GROUPS];
} cpumask_t;

STATIC_INLINE void cpumask_reset(cpumask_t* mask) {
	memset(mask, 0, sizeof(cpumask_t));
}

#define WIN_CPUMASK_PREAMBLE			\
	int groupid = cpuid >> 6;			\
	cpuid = cpuid & 0x3F;				\
	assert(groupid < MAX_CPU_GROUPS)

STATIC_INLINE void cpumask_set(cpumask_t* mask, int cpuid) {
	WIN_CPUMASK_PREAMBLE;

	mask->groups[groupid] |= 1ull << cpuid;
}

STATIC_INLINE void cpumask_clear(cpumask_t* mask, int cpuid) {
	WIN_CPUMASK_PREAMBLE;

	mask->groups[groupid] &= ~(1ull << cpuid);
}

STATIC_INLINE boolean_t cpumask_isset(cpumask_t* mask, int cpuid) {
	WIN_CPUMASK_PREAMBLE;

	return TO_BOOLEAN((mask->groups[groupid] & (1ull << cpuid)) != 0);
}

#endif /* PLAT_CPUMASK_H_ */
