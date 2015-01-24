
/*
    This file is part of TSLoad.
    Copyright 2013-2014, Sergey Klyaus, ITMO University

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



#ifndef RANDGEN_H_
#define RANDGEN_H_

/**
 * @module Random generators and variators
 *
 * API for creating random generators and variators
 */

#include <tsload/defs.h>
#include <tsload/autostring.h>
#include <tsload/modules.h>

#include <tsload/obj/obj.h>

#define RGHASHSIZE		8
#define RGHASHMASK		(RGHASHSIZE - 1)
#define RVHASHSIZE		8
#define RVHASHMASK		(RVHASHSIZE - 1)

struct randgen_class;

#define RG_ERROR_PREFIX 	"Failed to create random generator: "

typedef struct randgen {
	struct randgen_class* rg_class;
	uint64_t rg_seed;

	void* rg_private;
} randgen_t;

typedef void (*randgen_func_t)(randgen_t* rg);

/**
 * Random generator class.
 * 
 * To create new generator class, initialize head members with RG_CLASS_HEAD 
 * and set pointers to functions:
 * ```
 * randgen_class_t rg_my_class = {
 * 		RG_CLASS_HEAD("my", B_FALSE, max),
 * 		rg_init_my,
 *		rg_destroy_my,
 *		rg_generate_int 
 * };
 * ```
 * 
 * Note that TSLoad is not a cryptographic software, so predictability of PRNGs is not
 * a concern. It even have __sequental__ generator that generates numbers as N+1 where
 * N is last value.
 * 
 * Each random generated value is uint64_t. Real representation is irrelevant, i.e. for
 * /dev/urandom generator it is just a sequence of 8 bytes consequently read from device.
 * All conversion/limiting is done on a consumer side.
 * 
 * Random genrators should have uniform distribution in interval [0, rg_class->rg_max)
 * To apply different distribution, use random variators.
 * 
 * @member rg_class_name	name of class (used in configs)
 * @member rg_is_singleton	is random generator is signleton (relies on global state)
 * @member rg_ref_count 	reference count (for singleton)
 * @member rg_object		instance (for singleton)
 * @member rg_max			maximum integer that could be generated by this PRNG, i.e. LLONG_MAX
 * @member rg_next			next class in hashtable
 * @member rg_module		external module that implements this PRNG, for embedded generators 
 * 							from TSLoad Core (libtsload) this field is set NULL
 * @member rg_init			function that initializes PRNG internal state
 * @member rg_destroy		function that frees PRNG internal state resources
 * @member rg_generate_int	generate next random number
 */
typedef struct randgen_class {
	AUTOSTRING char* rg_class_name;
	
	boolean_t rg_is_singleton;
	int		  rg_ref_count;
	randgen_t* rg_object;

	uint64_t rg_max;
	
	struct randgen_class* rg_next;
	module_t* rg_module;

	int  (*rg_init)(randgen_t* rg);
	void (*rg_destroy)(randgen_t* rg);

	uint64_t (*rg_generate_int)(randgen_t* rg);
} randgen_class_t;

#define RG_CLASS_HEAD(name, is_singleton, max)		\
	SM_INIT(.rg_class_name, AAS_CONST_STR(name)),	\
	SM_INIT(.rg_is_singleton, is_singleton),		\
	SM_INIT(.rg_ref_count, 0),						\
	SM_INIT(.rg_object, NULL),						\
	SM_INIT(.rg_max, max),							\
	SM_INIT(.rg_next, NULL),						\
	SM_INIT(.rg_module, NULL)

LIBEXPORT randgen_t* rg_create(randgen_class_t* class, uint64_t seed);
LIBEXPORT void rg_destroy(randgen_t* rg);

/**
 * Generates integer with uniform distribution in range
 * [0; rg_class->rg_max]
 * */
STATIC_INLINE uint64_t rg_generate_int(randgen_t* rg) {
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

LIBEXPORT int randgen_register(module_t* mod, randgen_class_t* class);
LIBEXPORT int randgen_unregister(module_t* mod, randgen_class_t* class);

/**
 * Error codes returned by rv_set_int/rv_set_double
 */
#define	RV_PARAM_OK				0
#define RV_INVALID_PARAM_NAME	-1
#define RV_INVALID_PARAM_VALUE	-2

/**
 * Random variator parameter types
 */
#define RV_PARAM_NULL		0
#define RV_PARAM_INT		1
#define RV_PARAM_DOUBLE		2

struct randvar_class;

typedef struct randvar {
	randgen_t*	rv_generator;
	struct randvar_class* rv_class;
	void* rv_private;
} randvar_t;

typedef struct {
	int         type;
	const char* name;
	const char* helper;
} randvar_param_t;

/**
 * Random variator class
 * 
 * Random variators are not generating numbers. They take it from upper-level 
 * generator and modify it so probability funciton will conform desired distribution.
 * 
 * Random variators may have parameters of distribution settable via rv_set_int() and
 * rv_set_double() and described in rv_params (and of course in documentation).
 * 
 * Unlike random generators, variators work with double-precision floating point numbers
 * (`double` type) distributed in [0.0, 1.0]
 * 
 * Head members are initialized with RV_CLASS_HEAD()
 * 
 * @member rv_class_name	name of random variator class (for configs)
 * @member rv_next			next rv class in hashmap
 * @member rv_module		same as rg_module in randgen_class_t
 * @member rv_params		array of parameter descriptions 
 * @member rv_init			function that initializes internal state of variator
 * @member rv_destroy		function that frees its internal state
 * @member rv_set_int		function that sets integer parameter
 * @member rv_set_double 	function that sets double parameter
 * @member rv_variate_double function that gets random-generated value u, \
 * 							and returns variated value
 */
typedef struct randvar_class {
	AUTOSTRING char* rv_class_name;
	struct randvar_class* rv_next;
	module_t* rv_module;
	
	randvar_param_t* rv_params;
	
	int (*rv_init)(randvar_t* rv);
	void (*rv_destroy)(randvar_t* rv);

	int (*rv_set_int)(randvar_t* rv, const char* name, long value);
	int (*rv_set_double)(randvar_t* rv, const char* name, double value);

	double (*rv_variate_double)(randvar_t* rv, double u);
} randvar_class_t;

#define RV_CLASS_HEAD(name, params)					\
	SM_INIT(.rv_class_name, AAS_CONST_STR(name)),	\
	SM_INIT(.rv_next, NULL),						\
	SM_INIT(.rv_module, NULL),						\
	SM_INIT(.rv_params, params)

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

LIBEXPORT int randvar_register(module_t* mod, randvar_class_t* class);
LIBEXPORT int randvar_unregister(module_t* mod, randvar_class_t* class);

LIBEXPORT int randgen_init(void);
LIBEXPORT void randgen_fini(void);

LIBEXPORT randgen_t* tsobj_randgen_proc(tsobj_node_t* node);
LIBEXPORT randvar_t* tsobj_randvar_proc(tsobj_node_t* node, randgen_t* rg);

#endif /* RANDGEN_H_ */

