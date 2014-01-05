/*
 * cpumask.c
 *
 *  Created on: 15.06.2013
 *      Author: myaut
 */

#include <cpumask.h>
#include <mempool.h>

#include <plat/cpumask.h>

PLATAPI cpumask_t* cpumask_create() {
	size_t size = CPU_ALLOC_SIZE(MAX_CPUS);
	cpumask_t* mask = (cpumask_t*) mp_malloc(size + sizeof(size_t));

	mask->size = size;

	CPU_ZERO_S(mask->size, &mask->set);

	return mask;
}

void cpumask_destroy(cpumask_t* mask) {
	mp_free(mask);
}


PLATAPI boolean_t cpumask_contains(cpumask_t* a, cpumask_t* b) {
	cpumask_t* temp = cpumask_create();
	boolean_t ret = B_FALSE;

	CPU_OR_S(temp->size, &temp->set, &a->set, &b->set);
	ret = CPU_EQUAL_S(a->size, &a->set, &temp->set);

	cpumask_destroy(temp);

	return ret;
}

PLATAPI cpumask_t* cpumask_or(cpumask_t* a, cpumask_t* b) {
	cpumask_t* mask = cpumask_create();

	CPU_OR_S(a->size, &mask->set, &a->set, &b->set);

	return mask;
}
