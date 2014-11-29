#include <tsload/defs.h>

#include <tsload/mempool.h>
#include <tsload/time.h>

#include <tsload/load/rqsched.h>
#include <tsload.h>

#include <errormsg.h>

#include <string.h>


static rqsched_common_t* rqsched_common_create(size_t size) {
	rqsched_common_t* rqs = (rqsched_common_t*) mp_malloc(size);

	rqs->rqs_randgen = NULL;
	rqs->rqs_randvar = NULL;

	return rqs;
}

void rqsched_common_destroy(rqsched_common_t* rqs) {
	rv_destroy(rqs->rqs_randvar);
	rg_destroy(rqs->rqs_randgen);

	mp_free(rqs);
}

static int tsobj_rqsched_proc_randgen(tsobj_node_t* node, const char* param, randgen_t** p_randgen) {
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

static int tsobj_rqsched_proc_common(tsobj_node_t* node, workload_t* wl, rqsched_common_t* rqs) {
	json_node_t* randgen;
	char* distribution;

	int err;

	if(tsobj_get_string(node, "distribution", &distribution) != TSOBJ_OK)
		return RQSCHED_TSOBJ_BAD;

	rqs->rqs_randgen = NULL;
	rqs->rqs_randvar = NULL;

	err = tsobj_rqsched_proc_randgen(node, "randgen", &rqs->rqs_randgen);
	if(err != RQSCHED_TSOBJ_OK)
		return err;

	if(strcmp(distribution, "uniform") == 0) {
		double scope = 1.0;

		if(tsobj_get_double(node, "scope", &scope) != TSOBJ_OK) {
			return RQSCHED_TSOBJ_BAD;
		}

		if(scope < 0.0 || scope > 1.0) {
			tsload_error_msg(TSE_INVALID_VALUE,
							 RQSCHED_ERROR_PREFIX "invalid scope value %f",
							 wl->wl_name, scope);
			return RQSCHED_TSOBJ_ERROR;
		}

		rqs->rqs_params.u_scope = scope;

		rqs->rqs_randvar = rv_create(&rv_uniform_class, rqs->rqs_randgen);
		rqs->rqs_distribution = RQSD_UNIFORM;
	}
	else if(strcmp(distribution, "erlang") == 0) {
		int shape = 1.0;

		if(tsobj_get_integer_i(node, "shape", &shape) != TSOBJ_OK) {
			return RQSCHED_TSOBJ_BAD;
		}

		if(shape < 1) {
			tsload_error_msg(TSE_INVALID_VALUE,
							 RQSCHED_ERROR_PREFIX "invalid shape value %d",
							 wl->wl_name, shape);
			return RQSCHED_TSOBJ_ERROR;
		}

		rqs->rqs_params.e_shape = shape;

		rqs->rqs_randvar = rv_create(&rv_erlang_class, rqs->rqs_randgen);
		rqs->rqs_distribution = RQSD_ERLANG;

		rv_set_int(rqs->rqs_randvar, "shape", shape);
	}
	else if(strcmp(distribution, "exponential") == 0) {
		rqs->rqs_randvar = rv_create(&rv_exponential_class, rqs->rqs_randgen);
		rqs->rqs_distribution = RQSD_EXPONENTIAL;
	}
	else if(strcmp(distribution, "normal") == 0) {
		double dispersion = 1.0;

		if(tsobj_get_double(node, "dispersion", &dispersion) != TSOBJ_OK) {
			return RQSCHED_TSOBJ_BAD;
		}

		if(dispersion < 0.0) {
			tsload_error_msg(TSE_INVALID_VALUE,
							 RQSCHED_ERROR_PREFIX "invalid dispersion value %f",
							 wl->wl_name, dispersion);
			return RQSCHED_TSOBJ_ERROR;
		}

		rqs->rqs_params.n_dispersion = dispersion;

		rqs->rqs_randvar = rv_create(&rv_normal_class, rqs->rqs_randgen);
		rqs->rqs_distribution = RQSD_NORMAL;
	}
	else {
		tsload_error_msg(TSE_INVALID_VALUE,
						 RQSCHED_ERROR_PREFIX "invalid distribution '%s'",
						 wl->wl_name, distribution);
		return RQSCHED_TSOBJ_ERROR;
	}

	return RQSCHED_TSOBJ_OK;
}

static int tsobj_rqsched_proc_think(tsobj_node_t* node, workload_t* wl, rqsched_think_t* rqs_think) {
	if(tsobj_get_integer_i(node, "nusers", &rqs_think->rqs_nusers) != TSOBJ_OK)
		return RQSCHED_TSOBJ_BAD;

	if(rqs_think->rqs_nusers < 1) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 RQSCHED_ERROR_PREFIX "number of users %d must be 1 or greater",
						 wl->wl_name, rqs_think->rqs_nusers);
		return RQSCHED_TSOBJ_ERROR;
	}

	return tsobj_rqsched_proc_randgen(node, "user_randgen", &rqs_think->rqs_user_randgen);
}

int tsobj_rqsched_proc(tsobj_node_t* node, workload_t* wl) {
	int ret = RQSCHED_TSOBJ_OK;

	char* rqsched_type;

	rqsched_common_t* rqs = NULL;
	rqsched_think_t* rqs_think = NULL;

	if(tsobj_check_type(node, JSON_NODE) != TSOBJ_OK) {
		ret = RQSCHED_TSOBJ_BAD;
		goto end;
	}

	if(tsobj_get_string(node, "type", &rqsched_type) != TSOBJ_OK) {
		ret = RQSCHED_TSOBJ_BAD;
		goto end;
	}

	wl->wl_rqsched_private = NULL;

	if(strcmp(rqsched_type, "simple") == 0) {
		wl->wl_rqsched_class = &simple_rqsched_class;
	}
	else if(strcmp(rqsched_type, "iat") == 0) {
		wl->wl_rqsched_class = &iat_rqsched_class;

		rqs = rqsched_common_create(sizeof(rqsched_common_t));
		ret = tsobj_rqsched_proc_common(node, wl, rqs);

		if(ret == RQSCHED_TSOBJ_OK) {
			wl->wl_rqsched_private = (void*) rqs;
		}
	}
	else if(strcmp(rqsched_type, "think") == 0) {
		wl->wl_rqsched_class = &think_rqsched_class;

		rqs_think = (rqsched_think_t*) rqsched_common_create(sizeof(rqsched_think_t));
		rqs = &rqs_think->common;

		ret = tsobj_rqsched_proc_common(node, wl, rqs);

		if(ret == RQSCHED_TSOBJ_OK) {
			ret = tsobj_rqsched_proc_think(node, wl, rqs_think);

			if(ret == RQSCHED_TSOBJ_OK) {
				wl->wl_rqsched_private = (void*) rqs_think;
			}
		}
	}
	else {
		tsload_error_msg(TSE_INVALID_VALUE,
						 RQSCHED_ERROR_PREFIX "invalid type '%s'",
						 wl->wl_name, rqsched_type);
		ret = RQSCHED_TSOBJ_ERROR;
	}

	if(ret == RQSCHED_TSOBJ_OK && tsobj_check_unused(node) != TSOBJ_OK) {
		ret = RQSCHED_TSOBJ_BAD;
	}

end:
	if(ret != RQSCHED_TSOBJ_OK) {
		wl->wl_rqsched_class = NULL;
		wl->wl_rqsched_private = NULL;

		if(rqs != NULL) {
			if(rqs->rqs_randvar != NULL) {
				rv_destroy(rqs->rqs_randvar);
			}
			if(rqs->rqs_randgen != NULL) {
				rg_destroy(rqs->rqs_randgen);
			}

			mp_free(rqs);
		}
	}

	if(ret == RQSCHED_TSOBJ_BAD) {
		tsload_error_msg(tsobj_error_code(), RQSCHED_ERROR_PREFIX "%s",
								wl->wl_name, tsobj_error_message());
	}

	return ret;
}