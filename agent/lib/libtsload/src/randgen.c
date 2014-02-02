/*
 * randgen.c
 *
 *  Created on: Nov 14, 2013
 *      Author: myaut
 */

#include <defs.h>
#include <randgen.h>
#include <mempool.h>
#include <tstime.h>
#include <tsload.h>
#include <errcode.h>

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

randgen_t* json_randgen_proc(JSONNODE* node) {
	randgen_class_t* rg_class = NULL;
	char* rg_class_str;

	JSONNODE_ITERATOR i_class = json_find(node, "class"),
						  i_seed = json_find(node, "seed"),
						  i_end = json_end(node);

	uint64_t seed = (i_seed != i_end)
						? json_as_int(*i_seed)
						: tm_get_clock();

	if(i_class != i_end) {
		rg_class_str = json_as_string(*i_class);

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
			tsload_error_msg(TSE_INVALID_DATA, "Invalid random generator class '%s'", rg_class_str);
			json_free(rg_class_str);
			return NULL;
		}

		json_free(rg_class_str);
	}
	else {
		tsload_error_msg(TSE_INVALID_DATA, "Missing random generator class");
		return NULL;
	}

	return rg_create(rg_class, seed);
}

static randvar_t* json_randvar_proc_error(int ret, randvar_t* rv, const char* name) {
	if(ret == RV_INVALID_PARAM_NAME) {
		tsload_error_msg(TSE_INTERNAL_ERROR,
						 RV_ERROR_PREFIX ", parameter '%' not acceptable", name);
		rv_destroy(rv);
		return NULL;
	}
	if(ret == RV_INVALID_PARAM_VALUE) {
		tsload_error_msg(TSE_INVALID_DATA,
						 RV_ERROR_PREFIX", parameter '%' value is invalid", name);
		rv_destroy(rv);
		return NULL;
	}

	return rv;
}

#define RANDVAR_GET_PARAM_VALUE(iter, name, value, conv)\
	{													\
		iter = json_find(node, name);					\
		if(iter == i_end) {								\
			tsload_error_msg(TSE_MESSAGE_FORMAT,		\
					RV_ERROR_PREFIX ", missing parameter '%s'", name);	\
			return NULL;								\
														\
		}												\
		else if(json_type(*iter) != JSON_NUMBER) {		\
			tsload_error_msg(TSE_MESSAGE_FORMAT,		\
					RV_ERROR_PREFIX ", parameter '%' is not number");	\
			return NULL;								\
		}												\
		value = conv(*iter);							\
	}

#define RANDVAR_SET_PARAM_VALUE(rv, name, value, type)	\
	if(rv != NULL) {									\
		ret = rv_set_ ## type (rv, name, value);		\
		rv = json_randvar_proc_error(ret, rv, name);	\
	}

randvar_t* json_randvar_proc(JSONNODE* node, randgen_t* rg) {
	randvar_t* rv = NULL;
	char* rv_class_str;
	int ret;

	JSONNODE_ITERATOR i_class = json_find(node, "class"),
						  i_end = json_end(node);
	JSONNODE_ITERATOR i_param;

	if(i_class != i_end) {
		rv_class_str = json_as_string(*i_class);

		if(strcmp(rv_class_str, "exponential") == 0) {
			double rate;

			RANDVAR_GET_PARAM_VALUE(i_param, "rate", rate, json_as_float);

			rv = rv_create(&rv_exponential_class, rg);
			RANDVAR_SET_PARAM_VALUE(rv, "rate", rate, double);
		}
		else if(strcmp(rv_class_str, "uniform") == 0) {
			double min, max;

			RANDVAR_GET_PARAM_VALUE(i_param, "min", min, json_as_float);
			RANDVAR_GET_PARAM_VALUE(i_param, "max", max, json_as_float);

			rv = rv_create(&rv_uniform_class, rg);
			RANDVAR_SET_PARAM_VALUE(rv, "min", min, double);
			RANDVAR_SET_PARAM_VALUE(rv, "max", max, double);
		}
		else if(strcmp(rv_class_str, "erlang") == 0) {
			double rate;
			int shape;

			RANDVAR_GET_PARAM_VALUE(i_param, "rate", rate, json_as_float);
			RANDVAR_GET_PARAM_VALUE(i_param, "shape", rate, json_as_int);

			rv = rv_create(&rv_erlang_class, rg);
			RANDVAR_SET_PARAM_VALUE(rv, "rate", rate, double);
			RANDVAR_SET_PARAM_VALUE(rv, "shape", shape, int);
		}
		else if(strcmp(rv_class_str, "normal") == 0) {
			double mean, stddev;

			RANDVAR_GET_PARAM_VALUE(i_param, "mean", mean, json_as_float);
			RANDVAR_GET_PARAM_VALUE(i_param, "stddev", stddev, json_as_float);

			rv = rv_create(&rv_normal_class, rg);
			RANDVAR_SET_PARAM_VALUE(rv, "mean", mean, double);
			RANDVAR_SET_PARAM_VALUE(rv, "stddev", stddev, double);
		}
		else {
			tsload_error_msg(TSE_INVALID_DATA, "Invalid random variator class '%s'", rv_class_str);
			json_free(rv_class_str);
			return NULL;
		}

		json_free(rv_class_str);
	}

	return rv;
}

