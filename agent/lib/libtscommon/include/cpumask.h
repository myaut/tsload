/*
 * cpumask.h
 *
 *  Created on: 15.06.2013
 *      Author: myaut
 */

#ifndef CPUMASK_H_
#define CPUMASK_H_

#include <defs.h>

#include <plat/cpumask.h>

LIBEXPORT PLATAPI cpumask_t* cpumask_create();
LIBEXPORT PLATAPI void cpumask_destroy(cpumask_t* mask);

#ifndef HAVE_CPUMASK_BASIC
LIBEXPORT PLATAPI void cpumask_reset(cpumask_t* mask);
LIBEXPORT PLATAPI void cpumask_set(cpumask_t* mask, int cpuid);
LIBEXPORT PLATAPI void cpumask_clear(cpumask_t* mask, int cpuid);
LIBEXPORT PLATAPI boolean_t cpumask_isset(cpumask_t* mask, int cpuid);
#endif

#ifndef HAVE_CPUMASK_COUNT
LIBEXPORT PLATAPI int cpumask_count(cpumask_t* mask);
#endif

#ifndef HAVE_CPUMASK_EQ
LIBEXPORT PLATAPI boolean_t cpumask_eq(cpumask_t* a, cpumask_t* b);
#endif

#ifndef HAVE_CPUMASK_CONTAINS
LIBEXPORT PLATAPI boolean_t cpumask_contains(cpumask_t* a, cpumask_t* b);
#endif

#ifndef  HAVE_CPUMASK_OR
LIBEXPORT PLATAPI cpumask_t* cpumask_or(cpumask_t* a, cpumask_t* b);
#endif

#endif /* CPUMASK_H_ */
