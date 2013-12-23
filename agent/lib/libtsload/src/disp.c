#include <defs.h>

#include <disp.h>
#include <tsload.h>
#include <mempool.h>
#include <tstime.h>

#include <libjson.h>

#include <string.h>

extern disp_class_t simple_disp;
extern disp_class_t iat_disp;

void disp_common_destroy(disp_common_t* disp) {
	rv_destroy(disp->disp_randvar);
	rg_destroy(disp->disp_randgen);
}

static int json_disp_proc_common(JSONNODE* node, workload_t* wl, disp_common_t* disp) {
	int ret = DISP_JSON_OK;

	JSONNODE_ITERATOR i_randgen = json_find(node, "randgen"),
					  i_seed = json_find(node, "seed"),
					  i_distribution = json_find(node, "distribution"),
					  i_end = json_end(node);

	char* randgen;
	char* distribution;

	randgen_class_t* rg_class;
	uint64_t seed = (i_seed != i_end)
							? json_as_int(*i_seed)
							: tm_get_clock();

	if(i_distribution == i_end) {
		tsload_error_msg(TSE_MESSAGE_FORMAT, "Missing distribution for workload %s", wl->wl_name);
		return DISP_JSON_UNDEFINED;
	}

	disp->disp_randgen = NULL;
	disp->disp_randvar = NULL;

	if(i_randgen != i_end) {
		randgen = json_as_string(*i_randgen);

		if(strcmp(randgen, "libc") == 0) {
			rg_class = &rg_libc;
		}
		else {
			tsload_error_msg(TSE_INVALID_DATA, "Invalid random generator class '%s' for workload %s",
								randgen, wl->wl_name);
			json_free(randgen);
			return DISP_JSON_INVALID_PARAM;
		}

		json_free(randgen);
	}
	else {
		rg_class = &rg_libc;
	}

	disp->disp_randgen = rg_create(rg_class, seed);

	distribution = json_as_string(*i_distribution);

	if(strcmp(distribution, "uniform") == 0) {
		JSONNODE_ITERATOR i_scope = json_find(node, "scope");
		double scope = 1.0;

		if(i_scope != i_end) {
			scope = json_as_float(*i_scope);

			if(scope < 0.0 || scope > 1.0) {
				tsload_error_msg(TSE_INVALID_DATA, "Invalid scope value '%f' for workload %s",
												scope, wl->wl_name);
				ret = DISP_JSON_INVALID_PARAM;
				goto end;
			}
		}

		disp->disp_params.u_scope = scope;

		disp->disp_randvar = rv_create(&rv_uniform_class, disp->disp_randgen);
		disp->disp_distribution = DD_UNIFORM;
	}
	else if(strcmp(distribution, "erlang") == 0) {
		JSONNODE_ITERATOR i_shape = json_find(node, "shape");
		int shape = 1.0;

		if(i_shape != i_end) {
			shape = json_as_int(*i_shape);

			if(shape < 1) {
				tsload_error_msg(TSE_INVALID_DATA, "Invalid shape value '%d' for workload %s",
								  shape, wl->wl_name);
				ret = DISP_JSON_INVALID_PARAM;
				goto end;
			}
		}

		disp->disp_params.e_shape = shape;

		disp->disp_randvar = rv_create(&rv_erlang_class, disp->disp_randgen);
		disp->disp_distribution = DD_ERLANG;

		rv_set_int(disp->disp_randvar, "shape", shape);
	}
	else if(strcmp(distribution, "exponential") == 0) {
		disp->disp_randvar = rv_create(&rv_exponential_class, disp->disp_randgen);
		disp->disp_distribution = DD_EXPONENTIAL;
	}
	else if(strcmp(distribution, "normal") == 0) {
		JSONNODE_ITERATOR i_dispersion = json_find(node, "dispersion");
		double dispersion = 1.0;

		if(i_dispersion != i_end) {
			dispersion = json_as_float(*i_dispersion);

			if(dispersion < 0.0) {
				tsload_error_msg(TSE_INVALID_DATA, "Invalid dispersion value '%f' for workload %s",
						dispersion, wl->wl_name);
				ret = DISP_JSON_INVALID_PARAM;
				goto end;
			}
		}

		disp->disp_params.n_dispersion = dispersion;

		disp->disp_randvar = rv_create(&rv_normal_class, disp->disp_randgen);
		disp->disp_distribution = DD_NORMAL;
	}
	else {
		tsload_error_msg(TSE_INVALID_DATA, "Invalid distribution '%s' for workload %s",
							distribution, wl->wl_name);
		ret = DISP_JSON_INVALID_PARAM;
	}

end:
	if(ret != DISP_JSON_OK)
		rg_destroy(disp->disp_randgen);

	json_free(distribution);

	return ret;
}

int json_disp_proc(JSONNODE* node, workload_t* wl) {
	int ret = DISP_JSON_OK;

	JSONNODE_ITERATOR i_disp = json_find(node, "disp"),
					  i_disp_params = json_find(node, "disp_params"),
					  i_end = json_end(node);

	char* disp_name;
	disp_common_t* disp = NULL;

	if(i_disp == i_end) {
		tsload_error_msg(TSE_MESSAGE_FORMAT, "Missing dispatcher class for workload %s", wl->wl_name);
		return DISP_JSON_UNDEFINED;
	}

	disp_name = json_as_string(*i_disp);

	wl->wl_disp_private = NULL;

	if(strcmp(disp_name, "simple") == 0) {
		wl->wl_disp_class = &simple_disp;
	}
	else if(strcmp(disp_name, "iat") == 0) {
		wl->wl_disp_class = &iat_disp;

		if(i_disp_params != i_end) {
			disp = (disp_common_t*) mp_malloc(sizeof(disp_common_t));
			ret = json_disp_proc_common(*i_disp_params, wl, disp);

			if(ret == DISP_JSON_OK)
				wl->wl_disp_private = (void*) disp;
		}
		else {
			tsload_error_msg(TSE_MESSAGE_FORMAT, "Missing dispatcher parameters for workload %s", wl->wl_name);
			ret = DISP_JSON_UNDEFINED;
		}
	}
	else {
		tsload_error_msg(TSE_INVALID_DATA, "Invalid dispatcher class '%s' for workload %s",
							disp_name, wl->wl_name);
		ret = DISP_JSON_INVALID_CLASS;
	}

	if(ret != DISP_JSON_OK && disp != NULL)
		mp_free(disp);

	json_free(disp_name);
	return ret;
}
