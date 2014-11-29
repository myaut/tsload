
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

