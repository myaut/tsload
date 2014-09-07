/*
 * randgen.c
 *
 *  Created on: Nov 14, 2013
 *      Author: myaut
 */

#include <defs.h>
#include <errcode.h>

#include <mempool.h>
#include <tstime.h>

#include <tsload.h>
#include <randgen.h>

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

randgen_t* tsobj_randgen_proc(tsobj_node_t* node) {
	randgen_class_t* rg_class = NULL;
	char* rg_class_str;

	int error;
	int64_t seed;

	if(tsobj_get_string(node, "class", &rg_class_str) != TSOBJ_OK)
		goto bad_tsobj;

	error = tsobj_get_integer_i64(node, "seed", &seed);
	if(error == TSOBJ_INVALID_TYPE)
		goto bad_tsobj;

	if(tsobj_check_unused(node) != TSOBJ_OK)
		goto bad_tsobj;

	if(error == TSOBJ_NOT_FOUND)
		seed = tm_get_clock();

	if(strcmp(rg_class_str, "libc") == 0) {
		rg_class = &rg_libc_class;
	}
	else if(strcmp(rg_class_str, "seq") == 0) {
		rg_class = &rg_seq_class;
	}
	else if(strcmp(rg_class_str, "lcg") == 0) {
		rg_class = &rg_lcg_class;
	}
#ifdef PLAT_POSIX
	else if(strcmp(rg_class_str, "devrandom") == 0) {
		rg_class = &rg_devrandom_class;
	}
#endif
	else {
		tsload_error_msg(TSE_INVALID_VALUE,
						 RG_ERROR_PREFIX "invalid class '%s'", rg_class_str);
		return NULL;
	}

	return rg_create(rg_class, seed);

bad_tsobj:
	tsload_error_msg(tsobj_error_code(), RG_ERROR_PREFIX "%s", tsobj_error_message());

	return NULL;
}

static randvar_t* tsobj_randvar_proc_error(int ret, randvar_t* rv, const char* name,
			const char* range_helper) {
	if(ret == RV_INVALID_PARAM_NAME) {
		tsload_error_msg(TSE_INTERNAL_ERROR,
						 RV_ERROR_PREFIX "VALUE '%s' is not supported", name);
		rv_destroy(rv);
		return NULL;
	}
	if(ret == RV_INVALID_PARAM_VALUE) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 RV_ERROR_PREFIX "VALUE '%s' is invalid: %s",
						 name, range_helper);
		rv_destroy(rv);
		return NULL;
	}

	return rv;
}

#define RANDVAR_SET_PARAM_VALUE(rv, name, value, type, range_helper)	\
	if(rv != NULL) {													\
		ret = rv_set_ ## type (rv, name, value);						\
		rv = tsobj_randvar_proc_error(ret, rv, name, range_helper);		\
	}

randvar_t* tsobj_randvar_proc(tsobj_node_t* node, randgen_t* rg) {
	randvar_t* rv = NULL;
	char* rv_class_str;
	int ret;

	if(tsobj_get_string(node, "class", &rv_class_str) != TSOBJ_OK)
		goto bad_tsobj;

	if(strcmp(rv_class_str, "exponential") == 0) {
		double rate;

		if(tsobj_get_double_n(node, "rate", &rate) != TSOBJ_OK)
			goto bad_tsobj;
		if(tsobj_check_unused(node) != TSOBJ_OK)
			goto bad_tsobj;

		rv = rv_create(&rv_exponential_class, rg);
		RANDVAR_SET_PARAM_VALUE(rv, "rate", rate, double,
								"cannot be negative");
	}
	else if(strcmp(rv_class_str, "uniform") == 0) {
		double min, max;

		if(tsobj_get_double_n(node, "min", &min) != TSOBJ_OK)
			goto bad_tsobj;
		if(tsobj_get_double_n(node, "max", &max) != TSOBJ_OK)
			goto bad_tsobj;
		if(tsobj_check_unused(node) != TSOBJ_OK)
			goto bad_tsobj;

		rv = rv_create(&rv_uniform_class, rg);
		RANDVAR_SET_PARAM_VALUE(rv, "min", min, double, "");
		RANDVAR_SET_PARAM_VALUE(rv, "max", max, double, "");
	}
	else if(strcmp(rv_class_str, "erlang") == 0) {
		double rate;
		int shape;

		if(tsobj_get_double_n(node, "rate", &rate) != TSOBJ_OK)
			goto bad_tsobj;
		if(tsobj_get_integer_i(node, "shape", &shape) != TSOBJ_OK)
			goto bad_tsobj;
		if(tsobj_check_unused(node) != TSOBJ_OK)
			goto bad_tsobj;

		rv = rv_create(&rv_erlang_class, rg);
		RANDVAR_SET_PARAM_VALUE(rv, "rate", rate, double,
								"cannot be negative");
		RANDVAR_SET_PARAM_VALUE(rv, "shape", shape, int,
								"must be greater than 1");
	}
	else if(strcmp(rv_class_str, "normal") == 0) {
		double mean, stddev;

		if(tsobj_get_double_n(node, "mean", &mean) != TSOBJ_OK)
			goto bad_tsobj;
		if(tsobj_get_double_n(node, "stddev", &stddev) != TSOBJ_OK)
			goto bad_tsobj;
		if(tsobj_check_unused(node) != TSOBJ_OK)
			goto bad_tsobj;

		rv = rv_create(&rv_normal_class, rg);
		RANDVAR_SET_PARAM_VALUE(rv, "mean", mean, double, "");
		RANDVAR_SET_PARAM_VALUE(rv, "stddev", stddev, double,
								"should be positive");
	}
	else {
		tsload_error_msg(TSE_INVALID_VALUE,
						 RV_ERROR_PREFIX "invalid class '%s'", rv_class_str);
		return NULL;
	}

	return rv;

bad_tsobj:
	tsload_error_msg(tsobj_error_code(), RV_ERROR_PREFIX "%s", tsobj_error_message());

	return NULL;
}

#undef RANDVAR_SET_PARAM_VALUE
