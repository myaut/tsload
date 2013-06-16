/*
 * cpumask.h
 *
 *  Created on: 15.06.2013
 *      Author: myaut
 */

#ifndef PLAT_CPUMASK_H_
#define PLAT_CPUMASK_H_

#include <mempool.h>
#include <defs.h>

#include <stdlib.h>

#define STD_CPUS 			256
#define CPUS_PER_BUCKET		8

#define STD_BUCKETS		(STD_CPUS / CPUS_PER_BUCKET)

/* On Solaris affinity is done via processor sets, which require
 * processor ids, not masks, so it should be iterable.
 *
 * Simple array is not enough because operation on this have linear complexity.
 * Uses array of buckets, each of them contains up to 8 cpus masked in mask
 *
 * Also, modern SPARC machines are have a lots of strands (M5-32 - 1536 strands),
 * so cpumask may be dynamically reallocated */
typedef struct cpubucket {
	int  hibits;
	char mask;
} cpubucket_t;

typedef struct cpumask {
	int num_buckets;
	int max_buckets;

	size_t size;

	cpubucket_t* buckets;
} cpumask_t;

#endif /* PLAT_CPUMASK_H_ */
