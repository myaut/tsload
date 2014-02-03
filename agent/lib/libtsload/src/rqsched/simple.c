/*
 * simple.c
 *
 *  Created on: 04.12.2012
 *      Author: myaut
 */

#include <defs.h>

#include <rqsched.h>

void rqsched_fini_simple(workload_t* wl) {
	wl->wl_rqsched_private = NULL;
}

void rqsched_step_simple(workload_step_t* step) {
	/* NOTHING */
}

void rqsched_pre_request_simple(request_t* rq) {
	rq->rq_sched_time = rq->rq_workload->wl_time;
}

void rqsched_post_request_simple(request_t* rq) {
	/* NOTHING */
}

rqsched_class_t simple_rqsched_class = {
	RQSCHED_NAME("simple"),

	SM_INIT(.rqsched_fini, rqsched_fini_simple),
	SM_INIT(.rqsched_step, rqsched_step_simple),
	SM_INIT(.rqsched_pre_request, rqsched_pre_request_simple),
	SM_INIT(.rqsched_post_request, rqsched_post_request_simple)
};
