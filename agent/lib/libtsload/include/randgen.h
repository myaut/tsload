/*
 * randgen.h
 *
 *  Created on: Nov 14, 2013
 *      Author: myaut
 */

#ifndef RANDGEN_H_
#define RANDGEN_H_

/**
 * @module Random generators and variators
 *
 * API for creating random generators and variators
 */

#include <defs.h>

struct randgen_class;

#define RG_ERROR_PREFIX 	"Failed to create random generator: "

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

int  rg_init_dummy(randgen_t* rg);
void rg_destroy_dummy(randgen_t* rg);

/**
 * Random generators
 * */
LIBIMPORT randgen_class_t rg_libc_class;
LIBIMPORT randgen_class_t rg_seq_class;
LIBIMPORT randgen_class_t rg_lcg_class;
#ifdef PLAT_POSIX
LIBIMPORT randgen_class_t rg_devrandom_class;
#endif

#define	RV_PARAM_OK				0
#define RV_INVALID_PARAM_NAME	-1
#define RV_INVALID_PARAM_VALUE	-2

#define RV_ERROR_PREFIX 	"Failed to create random variator: "

struct randvar_class;

typedef struct randvar {
	randgen_t*	rv_generator;
	struct randvar_class* rv_class;
	void* rv_private;
} randvar_t;

typedef struct randvar_class {
	int (*rv_init)(randvar_t* rv);
	void (*rv_destroy)(randvar_t* rv);

	int (*rv_set_int)(randvar_t* rv, const char* name, long value);
	int (*rv_set_double)(randvar_t* rv, const char* name, double value);

	double (*rv_variate_double)(randvar_t* rv, double u);
} randvar_class_t;

LIBEXPORT randvar_t* rv_create(randvar_class_t* class, randgen_t* rg);
LIBEXPORT void rv_destroy(randvar_t* rv);

STATIC_INLINE int rv_set_int(randvar_t* rv, const char* name, long value) {
	return rv->rv_class->rv_set_int(rv, name, value);
}

STATIC_INLINE int rv_set_double(randvar_t* rv, const char* name, double value) {
	return rv->rv_class->rv_set_double(rv, name, value);
}

STATIC_INLINE double rv_variate_double(randvar_t* rv) {
	double u = rg_generate_double(rv->rv_generator);

	return rv->rv_class->rv_variate_double(rv, u);
}

int rv_init_dummy(randvar_t* rv);
void rv_destroy_dummy(randvar_t* rv);
int rv_set_int_dummy(randvar_t* rv, const char* name, long value);
int rv_set_double_dummy(randvar_t* rv, const char* name, double value);

LIBIMPORT randvar_class_t rv_uniform_class;
LIBIMPORT randvar_class_t rv_exponential_class;
LIBIMPORT randvar_class_t rv_erlang_class;
LIBIMPORT randvar_class_t rv_normal_class;

#ifdef NO_JSON
#include <tsobj.h>

LIBEXPORT randgen_t* tsobj_randgen_proc(tsobj_node_t* node);
LIBEXPORT randvar_t* tsobj_randvar_proc(tsobj_node_t* node, randgen_t* rg);
#else
STATIC_INLINE randgen_t* json_randgen_proc(void* node) { return NULL; }
STATIC_INLINE randvar_t* json_randvar_proc(void* node, randgen_t* rg) { return NULL; }
#endif

#endif /* RANDGEN_H_ */
