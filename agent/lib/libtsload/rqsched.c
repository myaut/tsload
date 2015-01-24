#include <tsload/defs.h>

#include <tsload/mempool.h>
#include <tsload/time.h>
#include <tsload/hashmap.h>

#include <tsload/load/rqsched.h>
#include <tsload.h>

#include <errormsg.h>

#include <string.h>

DECLARE_HASH_MAP_STRKEY(rqsvar_hash_map, rqsvar_class_t, RQSVARHASHSIZE, rqsvar_name, rqsvar_next, RQSVARHASHMASK);
DECLARE_HASH_MAP_STRKEY(rqsched_hash_map, rqsched_class_t, RQSVARHASHSIZE, rqsched_name, rqsched_next, RQSVARHASHMASK);

/**
 * #### Checking correctness of request scheduling with R
 * 
 *  * Create and run experiment
 *  * Export data to a csv
 *  * Run the following commands to plot PDF of scheduling times:
 * 		rq <- read.table('path/to/file.csv', sep=',', header=TRUE)
 * 		st <- sort(rq$rq_sched_time)
 * 		N <- length(st) - 2
 * 		plot(density(st[1:N+1] - st[0:N]))
 * 
 * PDF plot should match expected
 */

void rqsvar_destroy(rqsched_var_t* var) {
	if(var) {
		if(var->class->rqsvar_destroy)
			var->class->rqsvar_destroy(var);
		
		rv_destroy(var->randvar);
		rg_destroy(var->randgen);
		
		mp_free(var);
	}
}

void rqsched_destroy(workload_t* wl) {
	rqsched_t* rqs = (rqsched_t*) wl->wl_rqsched_private;
	
	rqs->rqs_class->rqsched_fini(wl, rqs);	
	
	rqsvar_destroy(rqs->rqs_var);
	mp_free(rqs);
	
	wl->wl_rqsched_class = NULL;
	wl->wl_rqsched_private = NULL;
}


int tsobj_rqsched_proc_randgen(tsobj_node_t* node, const char* param, randgen_t** p_randgen) {
	int err;
	tsobj_node_t* randgen;

	err = tsobj_get_node(node, param, &randgen);
	if(err == TSOBJ_INVALID_TYPE)
		return RQSCHED_TSOBJ_BAD;

	if(err == TSOBJ_OK) {
		*p_randgen = tsobj_randgen_proc(randgen);

		if(*p_randgen == NULL) {
			return RQSCHED_TSOBJ_RG_ERROR;
		}
	}
	else {
		*p_randgen = rg_create(&rg_lcg_class, tm_get_clock());
	}

	return RQSCHED_TSOBJ_OK;
}

int tsobj_rqsched_proc_variator(tsobj_node_t* node, workload_t* wl, rqsched_var_t** p_var) {
	rqsvar_class_t* var_class;
	rqsched_var_t* var = NULL;
	randvar_param_t* rv_param;
	
	json_node_t* randgen;
	char* distribution;

	int err;

	if(tsobj_get_string(node, "distribution", &distribution) != TSOBJ_OK)
		return RQSCHED_TSOBJ_BAD;

	var_class = (rqsvar_class_t*) hash_map_find(&rqsvar_hash_map, distribution);
	if(var_class == NULL) {
		tsload_error_msg(TSE_INVALID_VALUE, RQSCHED_ERROR_PREFIX "invalid distribution '%s'",
						 wl->wl_name, distribution);
		return RQSCHED_TSOBJ_ERROR;
	}
	
	var = mp_malloc(sizeof(rqsched_var_t));
	var->class = var_class;
	
	err = tsobj_rqsched_proc_randgen(node, "randgen", &var->randgen);
	if(err != RQSCHED_TSOBJ_OK) {
		mp_free(var);
		return err;
	}

	var->randvar = rv_create(var_class->rqsvar_rvclass, var->randgen);
	if(var->randvar == NULL) {
		rg_destroy(var->randgen);
		return RQSCHED_TSOBJ_ERROR;
	}
	
	if(var_class->rqsvar_init)
		var_class->rqsvar_init(var);
	
	rv_param = var_class->rqsvar_params;
	while(rv_param->type != RV_PARAM_NULL) {
		if(rv_param->type == RV_PARAM_INT) {
			long lval;
			if(tsobj_get_integer_l(node, rv_param->name, &lval) != TSOBJ_OK)
				goto bad_tsobj;
			if((err = var_class->rqsvar_set_int(var, rv_param->name, lval)) != RV_PARAM_OK)
				goto bad_param;
		}
		else if(rv_param->type == RV_PARAM_DOUBLE) {
			double dval;
			if(tsobj_get_double_n(node, rv_param->name, &dval) != TSOBJ_OK)
				goto bad_tsobj;
			if((err = var_class->rqsvar_set_double(var, rv_param->name, dval)) != RV_PARAM_OK)
				goto bad_param;
		}
		
		++rv_param;
	}
	
	*p_var = var;
	
	return RQSCHED_TSOBJ_OK;

bad_param:
	if(err == RV_INVALID_PARAM_NAME) {
		tsload_error_msg(TSE_INTERNAL_ERROR, RQSCHED_ERROR_PREFIX "PARAMETER '%s' is not supported", 
						 wl->wl_name, rv_param->name);
	}
	else if(err == RV_INVALID_PARAM_VALUE) {
		tsload_error_msg(TSE_INVALID_VALUE, RQSCHED_ERROR_PREFIX "PARAMETER '%s' has invalid value: %s", 
						 wl->wl_name, rv_param->name, rv_param->helper);
	}
	
	rqsvar_destroy(var);
	
	return RQSCHED_TSOBJ_ERROR;
	
bad_tsobj:
	rqsvar_destroy(var);
	
	return RQSCHED_TSOBJ_BAD;
}

int tsobj_rqsched_proc(tsobj_node_t* node, workload_t* wl) {
	rqsched_class_t* rqs_class = NULL;
	rqsched_t* rqs = NULL;
	int ret = RQSCHED_TSOBJ_OK;

	char* rqsched_type;

	wl->wl_rqsched_class = NULL;
	wl->wl_rqsched_private = NULL;
	
	if(tsobj_check_type(node, JSON_NODE) != TSOBJ_OK)
		goto bad_tsobj;

	if(tsobj_get_string(node, "type", &rqsched_type) != TSOBJ_OK)
		goto bad_tsobj;
	
	rqs_class = hash_map_find(&rqsched_hash_map, rqsched_type);	
	if(rqs_class == NULL) {
		tsload_error_msg(TSE_INVALID_VALUE, RQSCHED_ERROR_PREFIX "invalid type '%s'",
						 wl->wl_name, rqsched_type);
		return RQSCHED_TSOBJ_ERROR;
	}
	
	rqs = mp_malloc(sizeof(rqsched_t));
	rqs->rqs_class = rqs_class;
	rqs->rqs_private = NULL;
	rqs->rqs_var = NULL;	
	
	if(rqs_class->rqsched_flags & RQSCHED_NEED_VARIATOR) {
		ret = tsobj_rqsched_proc_variator(node, wl, &rqs->rqs_var);
		if(ret == RQSCHED_TSOBJ_ERROR || ret == RQSCHED_TSOBJ_RG_ERROR)
			goto error;
		if(ret == RQSCHED_TSOBJ_BAD)
			goto bad_tsobj;
	}
	
	if(rqs_class->rqsched_proc_tsobj != NULL) {
		ret = rqs_class->rqsched_proc_tsobj(node, wl, rqs);
		if(ret == RQSCHED_TSOBJ_ERROR)
			goto error;
		if(ret == RQSCHED_TSOBJ_BAD)
			goto bad_tsobj;
	}

	if(tsobj_check_unused(node) != TSOBJ_OK)
		goto bad_tsobj;
	
	
	wl->wl_rqsched_private = rqs;
	wl->wl_rqsched_class = rqs_class;

	return RQSCHED_TSOBJ_OK;

bad_tsobj:
	ret = RQSCHED_TSOBJ_BAD;
	tsload_error_msg(tsobj_error_code(), RQSCHED_ERROR_PREFIX "%s",
					wl->wl_name, tsobj_error_message());	
error:
	if(rqs) {
		if(rqs->rqs_var)
			rqsvar_destroy(rqs->rqs_var);
		mp_free(rqs);
	}
	return ret;
}

int rqsvar_register(module_t* mod, rqsvar_class_t* rqsvar_class) {
	rqsvar_class->rqsvar_next = NULL;
	rqsvar_class->rqsvar_module = mod;
	
	return hash_map_insert(&rqsvar_hash_map, rqsvar_class);
}

int rqsvar_unregister(module_t* mod, rqsvar_class_t* rqsvar_class) {
	return hash_map_remove(&rqsvar_hash_map, rqsvar_class);
}

int rqsched_register(module_t* mod, rqsched_class_t* rqs_class) {
	rqs_class->rqsched_next = NULL;
	rqs_class->rqsched_module = mod;
	
	return hash_map_insert(&rqsched_hash_map, rqs_class);
}

int rqsched_unregister(module_t* mod, rqsched_class_t* rqs_class) {
	return hash_map_remove(&rqsched_hash_map, rqs_class);
}

int rqsched_init(void) {
	hash_map_init(&rqsvar_hash_map, "rqsvar_hash_map");
	rqsvar_register(NULL, &rqsvar_exponential_class);
	rqsvar_register(NULL, &rqsvar_erlang_class);
	rqsvar_register(NULL, &rqsvar_uniform_class);
	rqsvar_register(NULL, &rqsvar_normal_class);
	
	hash_map_init(&rqsched_hash_map, "rqsched_hash_map");
	rqsched_register(NULL, &rqsched_simple_class);
	rqsched_register(NULL, &rqsched_iat_class);
	rqsched_register(NULL, &rqsched_think_class);
	
	return 0;
}

void rqsched_fini(void) {
	rqsched_unregister(NULL, &rqsched_think_class);
	rqsched_unregister(NULL, &rqsched_iat_class);
	rqsched_unregister(NULL, &rqsched_simple_class);
	hash_map_destroy(&rqsched_hash_map);
	
	rqsvar_unregister(NULL, &rqsvar_normal_class);
	rqsvar_unregister(NULL, &rqsvar_uniform_class);
	rqsvar_unregister(NULL, &rqsvar_erlang_class);
	rqsvar_unregister(NULL, &rqsvar_exponential_class);		
	hash_map_destroy(&rqsvar_hash_map);
}
