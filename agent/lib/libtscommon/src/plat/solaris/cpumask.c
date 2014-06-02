
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



#include <cpumask.h>

#include <strings.h>

#define CPUID_HIMASK		0xFFFFFFF8
#define CPUID_LOMASK(cpuid)	(1 << (cpuid & 0x7))

static int cpubucket_find(cpumask_t* mask, int cpuid) {
	int hibits = cpuid & CPUID_HIMASK;
	int bid;

	for(bid = 0; bid < mask->num_buckets; ++bid) {
		if(mask->buckets[bid].hibits == hibits)
			return bid;
	}

	return -1;
}

static int cpubucket_create(cpumask_t* mask, int cpuid) {
	int bid;

	/* No bucket slots left - reallocate cpumask */
	if(mask->num_buckets == mask->max_buckets) {
		mask->max_buckets += STD_BUCKETS;
		mask->size += STD_BUCKETS * sizeof(cpubucket_t);

		mask->buckets = mp_realloc(mask->buckets, mask->size);
	}

	bid = mask->num_buckets++;

	mask->buckets[bid].hibits = cpuid & CPUID_HIMASK;
	mask->buckets[bid].mask = 0;

	return bid;
}

PLATAPI cpumask_t* cpumask_create() {
	cpumask_t* mask = (cpumask_t*) mp_malloc(sizeof(cpumask_t));

	mask->num_buckets = 0;
	mask->max_buckets = STD_BUCKETS;

	mask->size = STD_BUCKETS * sizeof(cpubucket_t);

	mask->buckets = mp_malloc(mask->size);

	cpumask_reset(mask);

	return mask;
}

PLATAPI void cpumask_destroy(cpumask_t* mask) {
	mp_free(mask);
}

PLATAPI void cpumask_reset(cpumask_t* mask) {
	memset(mask->buckets, 0, mask->size);
}

PLATAPI void cpumask_set(cpumask_t* mask, int cpuid) {
	int bid = cpubucket_find(mask, cpuid);

	/* No such bucket found - create a new one */
	if(bid == -1) {
		bid = cpubucket_create(mask, cpuid);
	}

	mask->buckets[bid].mask |= CPUID_LOMASK(cpuid);
}

PLATAPI void cpumask_clear(cpumask_t* mask, int cpuid) {
	int bid = cpubucket_find(mask, cpuid);

	if(bid != -1) {
		mask->buckets[bid].mask &= ~CPUID_LOMASK(cpuid);
	}
}

PLATAPI boolean_t cpumask_isset(cpumask_t* mask, int cpuid) {
	int bid = cpubucket_find(mask, cpuid);

	if(bid == -1)
		return B_FALSE;

	return TO_BOOLEAN(mask->buckets[bid].mask & CPUID_LOMASK(cpuid));
}

PLATAPI int cpumask_count(cpumask_t* mask) {
	int bid;
	int count = 0;
	char bmask;

	static char bitcount[] = {0, 1, 1, 2,	/*0000 0001 0010 0011*/
							  1, 2, 2, 3,	/*0100 0101 0110 0111*/
							  1, 2, 2, 3,	/*1000 1001 1010 1011*/
							  2, 3, 3, 4,	/*1100 1101 1110 1111*/
							  };

	for(bid = 0; bid < mask->num_buckets; ++bid) {
		bmask = mask->buckets[bid].mask;
		count += bitcount[bmask >> 4] + bitcount[bmask & 0xF];
	}

	return count;
}

PLATAPI boolean_t cpumask_contains(cpumask_t* a, cpumask_t* b) {
	int abid, bbid;
	char amask, bmask;

	for(bbid = 0; bbid < b->num_buckets; ++bbid) {
		abid = cpubucket_find(a, b->buckets[bbid].hibits);

		/* Not even exists */
		if(abid == -1)
			return B_FALSE;

		amask = a->buckets[abid].mask;
		bmask = b->buckets[bbid].mask;

		if((amask & bmask) != amask)
			return B_FALSE;
	}

	return B_TRUE;
}

PLATAPI boolean_t cpumask_eq(cpumask_t* a, cpumask_t* b) {
	return cpumask_contains(a, b) && cpumask_contains(b, a);
}

PLATAPI cpumask_t* cpumask_or(cpumask_t* a, cpumask_t* b) {
	cpumask_t* newmask = cpumask_create();

	int bid, nbid;
	int hibits;
	char mask;

	for(bid = 0; bid < b->num_buckets; ++bid) {
		mask = b->buckets[bid].mask;
		hibits = b->buckets[bid].hibits;

		nbid = cpubucket_create(newmask, hibits);

		newmask->buckets[nbid].mask = mask;
	}

	for(bid = 0; bid < a->num_buckets; ++bid) {
		mask = a->buckets[bid].mask;
		hibits = a->buckets[bid].hibits;

		nbid = cpubucket_find(newmask, hibits);

		if(nbid == -1)
			nbid = cpubucket_create(newmask, hibits);

		newmask->buckets[nbid].mask |= mask;
	}

	return newmask;
}

