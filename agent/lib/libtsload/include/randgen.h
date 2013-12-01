/*
 * randgen.h
 *
 *  Created on: Nov 14, 2013
 *      Author: myaut
 */

#ifndef RANDGEN_H_
#define RANDGEN_H_

/**
 * randgen.h
 *
 * API for creating - random generators and variators
 */

#include <defs.h>

struct randgen_class;

typedef struct randgen {
	struct randgen_class* rg_class;
	uint64_t rg_seed;

	void* rg_private;
} randgen_t;

typedef void (*randgen_func_t)(randgen_t* rg);

typedef struct randgen_class {
	boolean_t rg_is_singleton;
	int		  rg_ref_count;
	randgen_t* rg_object;

	uint64_t rg_max;

	int  (*rg_init)(randgen_t* rg);
	void (*rg_destroy)(randgen_t* rg);

	uint64_t (*rg_generate_int)(randgen_t* rg);
} randgen_class_t;

#define RG_CLASS_HEAD(is_singleton, max)		\
	SM_INIT(.rg_is_singleton, is_singleton),	\
	SM_INIT(.rg_ref_count, 0),					\
	SM_INIT(.rg_object, NULL),					\
	SM_INIT(.rg_max, max)

LIBEXPORT randgen_t* rg_create(randgen_class_t* class, uint64_t seed);
LIBEXPORT void rg_destroy(randgen_t* rg);

/**
 * Generates integer with uniform distribution in range
 * [0; rg_class->rg_max]
 * */
static uint64_t rg_generate_int(randgen_t* rg) {
	return rg->rg_class->rg_generate_int(rg);
}

LIBEXPORT double rg_generate_double(randgen_t* rg);

LIBEXPORT int  rg_init_dummy(randgen_t* rg);
LIBEXPORT void rg_destroy_dummy(randgen_t* rg);

/**
 * Default random generator provided by standard library:
 * rand()/srand() & RAND_MAX
 * */
LIBIMPORT randgen_class_t rg_libc;

#define	RV_PARAM_OK				0
#define RV_INVALID_PARAM_NAME	-1
#define RV_INVALID_PARAM_VALUE	-2

struct randvar_class;

typedef struct randvar {
	randgen_t*	rv_generator;
	struct randvar_class* rv_class;
	void* rv_private;
} randvar_t;

typedef struct randvar_class {
	int (*rv_init)(randvar_t* rv);
	void (*rv_destroy)(randvar_t* rv);

	int (*rv_set_int)(randvar_t* rv, const char* name, uint64_t value);
	int (*rv_set_double)(randvar_t* rv, const char* name, double value);

	double (*rv_variate_double)(randvar_t* rv, double u);
} randvar_class_t;

LIBEXPORT randvar_t* rv_create(randvar_class_t* class, randgen_t* rg);
LIBEXPORT void rv_destroy(randvar_t* rv);

static int rv_set_int(randvar_t* rv, const char* name, uint64_t value) {
	return rv->rv_class->rv_set_int(rv, name, value);
}

static int rv_set_double(randvar_t* rv, const char* name, double value) {
	return rv->rv_class->rv_set_double(rv, name, value);
}

static double rv_variate_double(randvar_t* rv) {
	double u = rg_generate_double(rv->rv_generator);

	return rv->rv_class->rv_variate_double(rv, u);
}

LIBIMPORT randvar_class_t rv_uniform_class;
LIBIMPORT randvar_class_t rv_exponential_class;

#endif /* RANDGEN_H_ */
