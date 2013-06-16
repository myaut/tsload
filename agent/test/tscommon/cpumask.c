/*
 * cpumask.c
 *
 *  Created on: 15.06.2013
 *      Author: myaut
 */

#include <cpumask.h>

#include <stdio.h>

#include <assert.h>

#define CPUID1		100
#define CPUID2		200

#define CPUID3		150

void test_cpumask_basic(void) {
	cpumask_t* a = cpumask_create();

	/* test set / isset */
	cpumask_set(a, CPUID1);
	cpumask_set(a, CPUID2);
	cpumask_set(a, CPUID2);

	assert(cpumask_isset(a, CPUID1));
	assert(!cpumask_isset(a, CPUID3));

	/* test count */
	assert(cpumask_count(a) == 2);

	/* test clear */
	cpumask_clear(a, CPUID2);
	assert(!cpumask_isset(a, CPUID2));

	/* test reset */
	cpumask_reset(a);
	assert(!cpumask_isset(a, CPUID1));

	cpumask_destroy(a);
}

/* tests equals, contains */
void test_cpumask_cmp(void) {
	cpumask_t* a = cpumask_create();
	cpumask_t* b = cpumask_create();
	cpumask_t* c = cpumask_create();

	cpumask_set(a, CPUID1);
	cpumask_set(a, CPUID2);

	cpumask_set(b, CPUID1);
	cpumask_set(b, CPUID2);

	cpumask_set(c, CPUID1);
	cpumask_set(c, CPUID2);
	cpumask_set(c, CPUID3);

	assert(cpumask_eq(a, b));
	assert(cpumask_contains(c, b));

	cpumask_destroy(a);
	cpumask_destroy(b);
	cpumask_destroy(c);
}

void test_cpumask_or(void) {
	cpumask_t* a = cpumask_create();
	cpumask_t* b = cpumask_create();
	cpumask_t* c;

	cpumask_set(a, CPUID1);
	cpumask_set(a, CPUID2);

	cpumask_set(b, CPUID1);
	cpumask_set(b, CPUID3);

	c = cpumask_or(a, b);

	assert(cpumask_count(c) == 3);

	assert(cpumask_isset(c, CPUID1));
	assert(cpumask_isset(c, CPUID2));
	assert(cpumask_isset(c, CPUID3));

	cpumask_destroy(a);
	cpumask_destroy(b);
	cpumask_destroy(c);
}

#ifdef PLAT_SOLARIS
/* On Solaris we need to support a lots of cpus */
void test_cpumask_1024(void) {
	int cpuid = 0;
	int i;

	cpumask_t* a = cpumask_create();
	cpumask_t* b = cpumask_create();

	for(i = 0; i < 1024; ++i) {
		/* Revert quintets, so cpuid growth be non-linear */
		cpuid = (i >> 5) | ((i & 0x1F) << 5);

		cpumask_set(a, cpuid);

		if(i < 512)
			cpumask_set(b, cpuid);

#		if 0
		printf("%d %x A(%d,%d) B(%d,%d)\n", i, cpuid, a->max_buckets, a->num_buckets,
				b->max_buckets, b->num_buckets);
#		endif
	}

	assert(cpumask_contains(a, b));

	cpumask_destroy(a);
	cpumask_destroy(b);
}
#endif

int test_main() {
	test_cpumask_basic();
	test_cpumask_cmp();
	test_cpumask_or();

#	ifdef PLAT_SOLARIS
	test_cpumask_1024();
#	endif

	return 0;
}
