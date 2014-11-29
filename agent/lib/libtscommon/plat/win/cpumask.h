
/*
    This file is part of TSLoad.
    Copyright 2013, Sergey Klyaus, ITMO University

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



#ifndef PLAT_CPUMASK_H_
#define PLAT_CPUMASK_H_

#include <tsload/defs.h>

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

