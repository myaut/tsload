
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




#include <defs.h>

#include <rqsched.h>

#include <math.h>

/**
 * #### Inter-arrival time request scheduler
 *  */

static ts_time_t rqsched_iat_get_last_time(workload_t* wl, ts_time_t last_time) {
	request_t* last_rq;

	mutex_lock(&wl->wl_rq_mutex);
	if(!list_empty(&wl->wl_requests)) {
		last_rq = list_last_entry(request_t, &wl->wl_requests, rq_wl_node);
		last_time = last_rq->rq_sched_time;
	}
	mutex_unlock(&wl->wl_rq_mutex);

	return last_time;
}

void rqsched_fini_iat(workload_t* wl) {
	rqsched_common_t* disp = (rqsched_common_t*) wl->wl_rqsched_private;

	rqsched_common_destroy(disp);
}

void rqsched_step_iat(workload_step_t* step) {
	rqsched_common_t* rqs = (rqsched_common_t*) step->wls_workload->wl_rqsched_private;
	workload_t* wl = step->wls_workload;

	/* Compute mean inter-arrival time
	 * If previous step was overrun, correct inter-arrival time to reduce error
	 * Also because last request always overrun, add 1 to request count
	 * (this also protects from division to zero) */
	ts_time_t start_time = wl->wl_current_step * wl->wl_tp->tp_quantum;
	ts_time_t end_time = start_time + wl->wl_tp->tp_quantum;
	ts_time_t last_time = rqsched_iat_get_last_time(wl, start_time);

	double iat = (double) (end_time - max(last_time, start_time))/ (double) (step->wls_rq_count + 1);

	switch(rqs->rqs_distribution) {
	case RQSD_UNIFORM:
		rv_set_double(rqs->rqs_randvar, "min", (1.0 - rqs->rqs_params.u_scope) * iat);
		rv_set_double(rqs->rqs_randvar, "max", (1.0 + rqs->rqs_params.u_scope) * iat);
		break;
	case RQSD_EXPONENTIAL:
		rv_set_double(rqs->rqs_randvar, "rate", 1.0 / iat);
		break;
	case RQSD_ERLANG:
		rv_set_double(rqs->rqs_randvar, "rate", ((double) rqs->rqs_params.e_shape) / iat);
		break;
	case RQSD_NORMAL:
		rv_set_double(rqs->rqs_randvar, "mean", iat);
		rv_set_double(rqs->rqs_randvar, "stddev", sqrt(rqs->rqs_params.n_dispersion * iat));
		break;
	}
}

void rqsched_pre_request_iat(request_t* rq) {
	rqsched_common_t* rqs = (rqsched_common_t*) rq->rq_workload->wl_rqsched_private;
	workload_t* wl = rq->rq_workload;

	ts_time_t iat = (ts_time_t) rv_variate_double(rqs->rqs_randvar);

	ts_time_t last_time = rqsched_iat_get_last_time(wl, 0);
	ts_time_t start_time = wl->wl_current_step * wl->wl_tp->tp_quantum;

	rq->rq_sched_time = iat + max(last_time, start_time);
}

void rqsched_post_request_iat(request_t* rq) {
	/* NOTHING */
}

rqsched_class_t iat_rqsched_class = {
	RQSCHED_NAME("iat"),

	SM_INIT(.rqsched_fini, rqsched_fini_iat),
	SM_INIT(.rqsched_step, rqsched_step_iat),
	SM_INIT(.rqsched_pre_request, rqsched_pre_request_iat),
	SM_INIT(.rqsched_post_request, rqsched_post_request_iat)
};

