/*
 * iat.c
 *
 *  Created on: Nov 30, 2013
 *      Author: myaut
 */


#include <defs.h>

#include <disp.h>

#include <math.h>

/**
 * Inter-arrival time dispatcher
 *  */

void disp_fini_iat(workload_t* wl) {
	disp_common_t* disp = (disp_common_t*) wl->wl_disp_private;

	disp_common_destroy(disp);
}

void disp_step_iat(workload_step_t* step) {
	disp_common_t* disp = (disp_common_t*) step->wls_workload->wl_disp_private;

	/* Compute mean inter-arrival time */
	double iat = (double) step->wls_workload->wl_tp->tp_quantum / (double) step->wls_rq_count;

	switch(disp->disp_distribution) {
	case DD_UNIFORM:
		rv_set_double(disp->disp_randvar, "min", (1.0 - disp->disp_params.u_scope) * iat);
		rv_set_double(disp->disp_randvar, "max", (1.0 + disp->disp_params.u_scope) * iat);
		break;
	case DD_EXPONENTIAL:
		rv_set_double(disp->disp_randvar, "rate", 1.0 / iat);
		break;
	case DD_ERLANG:
		rv_set_double(disp->disp_randvar, "rate", ((double) disp->disp_params.e_shape) / iat);
		break;
	case DD_NORMAL:
		rv_set_double(disp->disp_randvar, "mean", iat);
		rv_set_double(disp->disp_randvar, "stddev", sqrt(disp->disp_params.n_dispersion * iat));
		break;
	}

	disp->last_iat = 0ull;
}

void disp_pre_request_iat(request_t* rq) {
	disp_common_t* disp = (disp_common_t*) rq->rq_workload->wl_disp_private;
	ts_time_t iat = (ts_time_t) rv_variate_double(disp->disp_randvar);
	ts_time_t delta = iat + disp->last_iat;

	disp->last_iat = delta;

	rq->rq_sched_time = delta + rq->rq_workload->wl_tp->tp_time;
}

void disp_post_request_iat(thread_pool_t* tp, request_t* rq) {
	/* NOTHING */
}

disp_class_t iat_disp = {
	DISP_NAME("iat"),

	SM_INIT(.disp_fini, disp_fini_iat),
	SM_INIT(.disp_step, disp_step_iat),
	SM_INIT(.disp_pre_request, disp_pre_request_iat),
	SM_INIT(.disp_post_request, disp_post_request_iat)
};
