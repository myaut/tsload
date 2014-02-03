#include <defs.h>

#include <rqsched.h>
#include <tsload.h>
#include <mempool.h>
#include <tstime.h>

#include <libjson.h>

#include <string.h>

extern rqsched_class_t simple_rqsched_class;
extern rqsched_class_t iat_rqsched_class;
extern rqsched_class_t think_rqsched_class;

void rqsched_common_destroy(rqsched_common_t* rqs) {
	rv_destroy(rqs->rqs_randvar);
	rg_destroy(rqs->rqs_randgen);

	mp_free(rqs);
}

static int json_rqsched_proc_common(JSONNODE* node, workload_t* wl, rqsched_common_t* rqs) {
	int ret = RQSCHED_JSON_OK;

	JSONNODE_ITERATOR i_randgen = json_find(node, "randgen"),
					  i_distribution = json_find(node, "distribution"),
					  i_end = json_end(node);

	char* distribution;

	if(i_distribution == i_end) {
		tsload_error_msg(TSE_MESSAGE_FORMAT, "Missing distribution for workload %s", wl->wl_name);
		return RQSCHED_JSON_UNDEFINED;
	}

	rqs->rqs_randgen = NULL;
	rqs->rqs_randvar = NULL;

	if(i_randgen != i_end) {
		rqs->rqs_randgen = json_randgen_proc(*i_randgen);

		if(rqs->rqs_randgen == NULL) {
			return RQSCHED_JSON_INVALID_PARAM;
		}
	}
	else {
		rqs->rqs_randgen = rg_create(&rg_lcg_class, tm_get_clock());
	}

	distribution = json_as_string(*i_distribution);

	if(strcmp(distribution, "uniform") == 0) {
		JSONNODE_ITERATOR i_scope = json_find(node, "scope");
		double scope = 1.0;

		if(i_scope != i_end) {
			scope = json_as_float(*i_scope);

			if(scope < 0.0 || scope > 1.0) {
				tsload_error_msg(TSE_INVALID_DATA, "Invalid scope value '%f' for workload %s",
												scope, wl->wl_name);
				ret = RQSCHED_JSON_INVALID_PARAM;
				goto end;
			}
		}

		rqs->rqs_params.u_scope = scope;

		rqs->rqs_randvar = rv_create(&rv_uniform_class, rqs->rqs_randgen);
		rqs->rqs_distribution = RQSD_UNIFORM;
	}
	else if(strcmp(distribution, "erlang") == 0) {
		JSONNODE_ITERATOR i_shape = json_find(node, "shape");
		int shape = 1.0;

		if(i_shape != i_end) {
			shape = json_as_int(*i_shape);

			if(shape < 1) {
				tsload_error_msg(TSE_INVALID_DATA, "Invalid shape value '%d' for workload %s",
								  shape, wl->wl_name);
				ret = RQSCHED_JSON_INVALID_PARAM;
				goto end;
			}
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
		JSONNODE_ITERATOR i_dispersion = json_find(node, "dispersion");
		double dispersion = 1.0;

		if(i_dispersion != i_end) {
			dispersion = json_as_float(*i_dispersion);

			if(dispersion < 0.0) {
				tsload_error_msg(TSE_INVALID_DATA, "Invalid dispersion value '%f' for workload %s",
						dispersion, wl->wl_name);
				ret = RQSCHED_JSON_INVALID_PARAM;
				goto end;
			}
		}

		rqs->rqs_params.n_dispersion = dispersion;

		rqs->rqs_randvar = rv_create(&rv_normal_class, rqs->rqs_randgen);
		rqs->rqs_distribution = RQSD_NORMAL;
	}
	else {
		tsload_error_msg(TSE_INVALID_DATA, "Invalid distribution '%s' for workload %s",
							distribution, wl->wl_name);
		ret = RQSCHED_JSON_INVALID_PARAM;
	}

end:
	if(ret != RQSCHED_JSON_OK)
		rg_destroy(rqs->rqs_randgen);

	json_free(distribution);

	return ret;
}

static int json_rqsched_proc_think(JSONNODE* node, workload_t* wl, rqsched_think_t* rqs_think) {
	int ret = RQSCHED_JSON_OK;

	JSONNODE_ITERATOR i_randgen = json_find(node, "user_randgen"),
					  i_nusers = json_find(node, "nusers"),
					  i_end = json_end(node);

	if(i_nusers == i_end) {
		tsload_error_msg(TSE_MESSAGE_FORMAT, "Missing number of users for workload %s", wl->wl_name);
		return RQSCHED_JSON_UNDEFINED;
	}

	if(json_type(*i_nusers) != JSON_NUMBER) {
		tsload_error_msg(TSE_MESSAGE_FORMAT, "'nusers' for workload %s should be JSON_NUMBER", wl->wl_name);
		return RQSCHED_JSON_INVALID_PARAM;
	}

	rqs_think->rqs_nusers = json_as_int(*i_nusers);

	if(i_randgen != i_end) {
		rqs_think->rqs_user_randgen = json_randgen_proc(*i_randgen);

		if(rqs_think->rqs_user_randgen == NULL) {
			return RQSCHED_JSON_INVALID_PARAM;
		}
	}
	else {
		rqs_think->rqs_user_randgen = rg_create(&rg_lcg_class, tm_get_clock());
	}

	return RQSCHED_JSON_OK;
}

int json_rqsched_proc(JSONNODE* node, workload_t* wl) {
	int ret = RQSCHED_JSON_OK;

	JSONNODE_ITERATOR i_rqsched = json_find(node, "rqsched"),
					  i_rqsched_params = json_find(node, "rqsched_params"),
					  i_end = json_end(node);

	char* rqsched_name;
	rqsched_common_t* rqs = NULL;
	rqsched_think_t* rqs_think = NULL;

	if(i_rqsched == i_end) {
		tsload_error_msg(TSE_MESSAGE_FORMAT,
						 "Missing request scheduler class for workload %s", wl->wl_name);
		return RQSCHED_JSON_UNDEFINED;
	}

	rqsched_name = json_as_string(*i_rqsched);

	wl->wl_rqsched_private = NULL;

	if(strcmp(rqsched_name, "simple") == 0) {
		wl->wl_rqsched_class = &simple_rqsched_class;
	}
	else if(strcmp(rqsched_name, "iat") == 0) {
		wl->wl_rqsched_class = &iat_rqsched_class;

		if(i_rqsched_params != i_end) {
			rqs = (rqsched_common_t*) mp_malloc(sizeof(rqsched_common_t));
			ret = json_rqsched_proc_common(*i_rqsched_params, wl, rqs);

			if(ret == RQSCHED_JSON_OK) {
				wl->wl_rqsched_private = (void*) rqs;
			}
		}
		else {
			ret = RQSCHED_JSON_MISSING_PARAMS;
		}
	}
	else if(strcmp(rqsched_name, "think") == 0) {
		wl->wl_rqsched_class = &think_rqsched_class;

		if(i_rqsched_params != i_end) {
			rqs_think = (rqsched_think_t*) mp_malloc(sizeof(rqsched_think_t));
			rqs = &rqs_think->common;

			ret = json_rqsched_proc_common(*i_rqsched_params, wl, rqs);

			if(ret == RQSCHED_JSON_OK) {
				ret = json_rqsched_proc_think(*i_rqsched_params, wl, rqs_think);

				wl->wl_rqsched_private = (void*) rqs_think;
			}
		}
		else {
			ret = RQSCHED_JSON_MISSING_PARAMS;
		}
	}
	else {
		tsload_error_msg(TSE_INVALID_DATA,
						 "Invalid request scheduler class '%s' for workload %s",
						  rqsched_name, wl->wl_name);
		ret = RQSCHED_JSON_INVALID_CLASS;
	}

	if(ret == RQSCHED_JSON_MISSING_PARAMS) {
		tsload_error_msg(TSE_MESSAGE_FORMAT,
						 "Missing request scheduler parameters for workload %s", wl->wl_name);
	}

	if(ret != RQSCHED_JSON_OK && rqs != NULL)
		mp_free(rqs);

	json_free(rqsched_name);
	return ret;
}
