/*
 * cpumask.c
 *
 *  Created on: 15.06.2013
 *      Author: myaut
 */

#include <mempool.h>

#include <cpumask.h>

PLATAPI cpumask_t* cpumask_create(void) {
	cpumask_t* mask = (cpumask_t*) mp_malloc(sizeof(cpumask_t));

	cpumask_reset(mask);

	return mask;
}

PLATAPI void cpumask_destroy(cpumask_t* mask) {
	mp_free(mask);
}

PLATAPI int cpumask_count(cpumask_t* mask) {
	int groupid = 0;
	int count = 0;
	uint64_t probe = 0;

	for(groupid = 0; groupid < MAX_CPU_GROUPS; ++groupid) {
		if(mask->groups[groupid] == 0ull)
			continue;

		/* FIXME: Need fast bitcount algorithm for 64-bits integers */
		for(probe = 1ull;
			probe < (1ull << 63);
			probe <<= 1) {
				if((mask->groups[groupid] & probe) != 0ull)
					++count;
		}
	}

	return count;
}

PLATAPI boolean_t cpumask_eq(cpumask_t* a, cpumask_t* b) {
	int groupid = 0;

	for(groupid = 0; groupid < MAX_CPU_GROUPS; ++groupid) {
		if(a->groups[groupid] != b->groups[groupid])
			return B_FALSE;
	}

	return B_TRUE;
}

PLATAPI boolean_t cpumask_contains(cpumask_t* a, cpumask_t* b) {
	int groupid = 0;

	uint64_t amask;
	uint64_t bmask;

	for(groupid = 0; groupid < MAX_CPU_GROUPS; ++groupid) {
		amask = a->groups[groupid];
		bmask = b->groups[groupid];

		if((amask | bmask) != amask)
			return B_FALSE;
	}

	return B_TRUE;
}

PLATAPI cpumask_t* cpumask_or(cpumask_t* a, cpumask_t* b) {
	cpumask_t* newmask = cpumask_create();
	int groupid;

	for(groupid = 0; groupid < MAX_CPU_GROUPS; ++groupid) {
		newmask->groups[groupid] = a->groups[groupid] | b->groups[groupid];
	}

	return newmask;
}
