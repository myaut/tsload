/*
 * randgen.c
 *
 *  Created on: Nov 14, 2013
 *      Author: myaut
 */

#include <defs.h>
#include <randgen.h>
#include <mempool.h>

#include <string.h>

randgen_t* rg_create(randgen_class_t* class, uint64_t seed) {
	randgen_t* rg;

	if(class->rg_is_singleton && (class->rg_object != NULL)) {
		++class->rg_ref_count;
		return class->rg_object;
	}

	rg = (randgen_t*) mp_malloc(sizeof(randgen_t));

	if(rg == NULL)
		return NULL;

	rg->rg_class = class;
	rg->rg_seed  = seed;
	rg->rg_private = NULL;

	if(class->rg_init(rg) != 0) {
		mp_free(rg);
		return NULL;
	}

	++class->rg_ref_count;
	class->rg_object = rg;

	return rg;
}

void rg_destroy(randgen_t* rg) {
	if(--rg->rg_class->rg_ref_count == 0) {
		rg->rg_class->rg_destroy(rg);
		mp_free(rg);
	}
}

/**
 * Generates double with uniform distribution in range
 * [0.0; 1.0]
 * */
double rg_generate_double(randgen_t* rg) {
	/* By raw conversion integer to doubles we may lose precision
	 * and create bad sequences. So do the integer division and
	 * convert to double then do the double calculations:
	 *
	 * Let m - rand_max, r - random number (int), u - random number (double)
	 *
	 * x = m div r, y = m mod r (only valid ops with ints)
	 * Then:
	 *      r       1             1               1
	 * u = --- = ------ = ---------------- = -----------
	 *      m     m / r    (x * r + y) / r    x + (y / r)
	 *
	 * NOTE: Testing on Python 2.7 gives very slight difference (about 1e-16), cause double is
	 * pretty precise type. So, it's a configuration option
	 *  */
#ifndef TSLOAD_RANDGEN_FAST
	uint64_t r = rg_generate_int(rg);
	uint64_t x, y;
	double   u;

	if(r == 0)
		return 0.0;

	x = rg->rg_class->rg_max / r;
	y = rg->rg_class->rg_max % r;

	u = 1.0 / ((double)x + ((double)y / (double)r));

	return u;
#else
	return ((double) rg_generate_int(rg)) / ((double) rg->rg_class->rg_max);
#endif
}

int rg_init_dummy(randgen_t* rg) {
	return 0;
}

void rg_destroy_dummy(randgen_t* rg) {
}

/* Variator common functions */

randvar_t* rv_create(randvar_class_t* class, randgen_t* rg) {
	randvar_t* rv = (randvar_t*) mp_malloc(sizeof(randvar_t));

	if(rv == NULL)
		return rv;

	rv->rv_class = class;
	rv->rv_generator = rg;
	rv->rv_private = NULL;

	if(class->rv_init(rv) != 0) {
		mp_free(rv);
		return NULL;
	}

	return rv;
}

void rv_destroy(randvar_t* rv) {
	rv->rv_class->rv_destroy(rv);

	mp_free(rv);
}

int rv_init_dummy(randvar_t* rv) {
	return 0;
}

void rv_destroy_dummy(randvar_t* rv) {
}

int rv_set_int_dummy(randvar_t* rv, const char* name, long value) {
	return RV_INVALID_PARAM_NAME;
}

int rv_set_double_dummy(randvar_t* rv, const char* name, double value) {
	return RV_INVALID_PARAM_NAME;
}

