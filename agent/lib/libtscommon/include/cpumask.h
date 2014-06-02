
/*
    This file is part of TSLoad.
    Copyright 2013-2014, Sergey Klyaus, ITMO University

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



#ifndef CPUMASK_H_
#define CPUMASK_H_

#include <defs.h>

#include <plat/cpumask.h>

/**
 * @module CPU masks
 *
 * CPU masks are used in sched_get_affinity()/sched_set_affinitity() functions
 * they contain set of cpuid's, on that thread is allowed to run.
 *
 * cpuid is id of strand object taken from libhostinfo
 */

/**
 * Allocate and initialize new CPU mask */
LIBEXPORT PLATAPI cpumask_t* cpumask_create();

/**
 * Destroy CPU mask
 * */
LIBEXPORT PLATAPI void cpumask_destroy(cpumask_t* mask);

#ifndef HAVE_CPUMASK_BASIC

/**
 * Remove all CPUs from mask*/
LIBEXPORT PLATAPI void cpumask_reset(cpumask_t* mask);

/**
 * Add CPU to mask */
LIBEXPORT PLATAPI void cpumask_set(cpumask_t* mask, int cpuid);

/**
 * Remove CPU from mask */
LIBEXPORT PLATAPI void cpumask_clear(cpumask_t* mask, int cpuid);

/**
 * Test to see if CPU is inside this mask */
LIBEXPORT PLATAPI boolean_t cpumask_isset(cpumask_t* mask, int cpuid);
#endif

#ifndef HAVE_CPUMASK_COUNT
/**
 * Returns count of CPUs in mask */
LIBEXPORT PLATAPI int cpumask_count(cpumask_t* mask);
#endif

#ifndef HAVE_CPUMASK_EQ
/**
 * Returns B_TRUE if masks a and b are equal */
LIBEXPORT PLATAPI boolean_t cpumask_eq(cpumask_t* a, cpumask_t* b);
#endif

#ifndef HAVE_CPUMASK_CONTAINS
/**
 * Returns B_TRUE if b is submask of a  */
LIBEXPORT PLATAPI boolean_t cpumask_contains(cpumask_t* a, cpumask_t* b);
#endif

#ifndef  HAVE_CPUMASK_OR
/**
 * Returns new CPU mask that contains CPUs that are included in
 * both a and b masks (similiar to boolean or)
 *
 * @note Resulting mask should be freed with cpumask_destroy() */
LIBEXPORT PLATAPI cpumask_t* cpumask_or(cpumask_t* a, cpumask_t* b);
#endif

#endif /* CPUMASK_H_ */

