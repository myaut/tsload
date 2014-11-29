
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

#include <tsload/mempool.h>

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

