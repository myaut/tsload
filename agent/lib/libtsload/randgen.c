
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



#include <tsload/defs.h>

#include <tsload/errcode.h>
#include <tsload/mempool.h>
#include <tsload/time.h>
#include <tsload/hashmap.h>

#include <tsload.h>
#include <tsload/load/randgen.h>

#include <errormsg.h>
#include <tsloadimpl.h>

#include <string.h>

DECLARE_HASH_MAP_STRKEY(randgen_hash_map, randgen_class_t, RGHASHSIZE, rg_class_name, rg_next, RGHASHMASK);
DECLARE_HASH_MAP_STRKEY(randvar_hash_map, randvar_class_t, RGHASHSIZE, rv_class_name, rv_next, RVHASHMASK);

extern randvar_class_t rv_uniform_class;
extern randvar_class_t rv_exponential_class;
extern randvar_class_t rv_erlang_class;
extern randvar_class_t rv_normal_class;

extern randgen_class_t rg_libc_class;
extern randgen_class_t rg_seq_class;
extern randgen_class_t rg_lcg_class;
#ifdef PLAT_POSIX
extern randgen_class_t rg_devrandom_class;
#endif

randgen_t* rg_create(randgen_class_t* class, uint64_t seed) {
	randgen_t* rg;

	if(class->rg_is_singleton && atomic_inc(&class->rg_ref_count) > 0l) {
		/* Wait till other side will set class->rg_object */
		while(!class->rg_object) 
			atomic_read(&class->rg_ref_count);
		
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

	class->rg_object = rg;

	return rg;
}

void rg_destroy(randgen_t* rg) {
	randgen_class_t* class = rg->rg_class;
	
	if(!class->rg_is_singleton || atomic_dec(&class->rg_ref_count) == 1l) {
		class->rg_destroy(rg);
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

tsobj_node_t* tsobj_randgen_class_format(randgen_class_t* rg_class) {
	AUTOSTRING char* max_value_str;
	tsobj_node_t* node = tsobj_new_node("tsload.randgen.GeneratorClass");
	
	tsobj_add_boolean(node, TSOBJ_STR("is_singleton"), rg_class->rg_is_singleton);
	
	/* uint64_t cannot be represented by libtsjson, so use string here */
	aas_printf(aas_init(&max_value_str), "%" PRIu64, rg_class->rg_max);
	tsobj_add_string(node, TSOBJ_STR("max_value"), 
					 tsobj_str_create(max_value_str));
	aas_free(&max_value_str);
	
	tsobj_module_format_helper(node, rg_class->rg_module);
	
	return node;
}

randgen_t* tsobj_randgen_proc(tsobj_node_t* node) {
	randgen_class_t* rg_class = NULL;
	const char* rg_class_name;

	int error;
	int64_t seed;

	if(tsobj_get_string(node, "class", &rg_class_name) != TSOBJ_OK)
		goto bad_tsobj;

	error = tsobj_get_integer_i64(node, "seed", &seed);
	if(error == TSOBJ_INVALID_TYPE)
		goto bad_tsobj;

	if(tsobj_check_unused(node) != TSOBJ_OK)
		goto bad_tsobj;

	if(error == TSOBJ_NOT_FOUND)
		seed = tm_get_clock();

	rg_class = (randgen_class_t*) hash_map_find(&randgen_hash_map, rg_class_name);
	if(rg_class == NULL) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 RG_ERROR_PREFIX "invalid class '%s'", rg_class_name);
		return NULL;
	}

	return rg_create(rg_class, seed);

bad_tsobj:
	tsload_error_msg(tsobj_error_code(), RG_ERROR_PREFIX "%s", tsobj_error_message());

	return NULL;
}

tsobj_node_t* tsobj_randvar_class_format(randvar_class_t* rv_class) {
	tsobj_node_t* node = tsobj_new_node("tsload.randgen.VariatorClass");
	
	tsobj_add_node(node, TSOBJ_STR("params"), 
		tsobj_params_format_helper(rv_class->rv_params));
	tsobj_module_format_helper(node, rv_class->rv_module);
	
	return node;
}

randvar_t* tsobj_randvar_proc(tsobj_node_t* node, randgen_t* rg) {
	randvar_t* rv = NULL;
	randvar_class_t* rv_class;
	tsload_param_t* rv_param;
	const char* rv_class_name;
	int ret = RV_PARAM_OK;
	
	if(tsobj_get_string(node, "class", &rv_class_name) != TSOBJ_OK)
		goto bad_tsobj;

	rv_class = hash_map_find(&randvar_hash_map, rv_class_name);	
	if(rv_class == NULL) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 RV_ERROR_PREFIX "invalid class '%s'", rv_class_name);
		return NULL;
	}
	
	rv = rv_create(rv_class, rg);
	if(rv == NULL) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 RV_ERROR_PREFIX "failed to initialize random variator");
		return NULL;
	}
	
	rv_param = rv_class->rv_params;
	while(rv_param->type != TSLOAD_PARAM_NULL) {
		if(rv_param->type == TSLOAD_PARAM_INTEGER) {
			long lval;
			if(tsobj_get_integer_l(node, rv_param->name, &lval) != TSOBJ_OK)
				goto bad_tsobj;
			if((ret = rv_set_int(rv, rv_param->name, lval)) != RV_PARAM_OK)
				goto bad_param;
		}
		else if(rv_param->type == TSLOAD_PARAM_FLOAT) {
			double dval;
			if(tsobj_get_double_n(node, rv_param->name, &dval) != TSOBJ_OK)
				goto bad_tsobj;
			if((ret = rv_set_double(rv, rv_param->name, dval)) != RV_PARAM_OK)
				goto bad_param;
		}
		
		++rv_param;
	}
	
	if(tsobj_check_unused(node) != TSOBJ_OK)
		goto bad_tsobj;

	return rv;

bad_param:
	if(ret == RV_INVALID_PARAM_NAME) {
		tsload_error_msg(TSE_INTERNAL_ERROR,
						 RV_ERROR_PREFIX "PARAMETER '%s' is not supported", rv_param->name);
	}
	else if(ret == RV_INVALID_PARAM_VALUE) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 RV_ERROR_PREFIX "PARAMETER '%s' has invalid value: %s", 
						 rv_param->name, rv_param->hint);
	}
	
	rv_destroy(rv);
	return NULL;
	
bad_tsobj:
	tsload_error_msg(tsobj_error_code(), RV_ERROR_PREFIX "%s", tsobj_error_message());
	
	if(rv != NULL)
		rv_destroy(rv);
	return NULL;
}

randgen_class_t* randgen_find(const char* class_name) {
	return hash_map_find(&randgen_hash_map, class_name);
}

int randgen_register(module_t* mod, randgen_class_t* class) {
	class->rg_module = mod;
	
	return hash_map_insert(&randgen_hash_map, class);
}

int randgen_unregister(module_t* mod, randgen_class_t* class) {
	return hash_map_remove(&randgen_hash_map, class);
}

randvar_class_t* randvar_find(const char* class_name) {
	return hash_map_find(&randvar_hash_map, class_name);
}

int randvar_register(module_t* mod, randvar_class_t* class) {
	class->rv_module = mod;
	
	return hash_map_insert(&randvar_hash_map, class);
}

int randvar_unregister(module_t* mod, randvar_class_t* class) {
	return hash_map_remove(&randvar_hash_map, class);
}


int randgen_init(void) {
	hash_map_init(&randgen_hash_map, "randgen_hash_map");
	hash_map_init(&randvar_hash_map, "randvar_hash_map");
	
	randgen_register(NULL, &rg_libc_class);
	randgen_register(NULL, &rg_lcg_class);
	randgen_register(NULL, &rg_seq_class);
#ifdef PLAT_POSIX
	randgen_register(NULL, &rg_devrandom_class);
#endif
	
	randvar_register(NULL, &rv_erlang_class);
	randvar_register(NULL, &rv_exponential_class);
	randvar_register(NULL, &rv_normal_class);
	randvar_register(NULL, &rv_uniform_class);
	
	return 0;
}

void randgen_fini(void) {
	randvar_unregister(NULL, &rv_uniform_class);
	randvar_unregister(NULL, &rv_normal_class);
	randvar_unregister(NULL, &rv_exponential_class);
	randvar_unregister(NULL, &rv_erlang_class);	
	
#ifdef PLAT_POSIX
	randgen_unregister(NULL, &rg_devrandom_class);
#endif	
	randgen_unregister(NULL, &rg_seq_class);
	randgen_unregister(NULL, &rg_lcg_class);
	randgen_unregister(NULL, &rg_libc_class);
	
	hash_map_destroy(&randvar_hash_map);
	hash_map_destroy(&randgen_hash_map);
}