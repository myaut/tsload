/*
 * think.c
 *
 *  Created on: Feb 2, 2014
 *      Author: myaut
 */

#include <defs.h>

#include <tstime.h>
#include <randgen.h>
#include <rqsched.h>
#include <tpdisp.h>
#include <threadpool.h>

#include <math.h>

/**
 * Think-time based request scheduler
 *  */

extern void rqsched_pre_request_iat(request_t* rq);
extern void rqsched_step_iat(workload_step_t* step);

void rqsched_fini_think(workload_t* wl) {
	rqsched_think_t* rqs_think = (rqsched_think_t*) wl->wl_rqsched_private;

	rg_destroy(rqs_think->rqs_user_randgen);

	/* rqsched_common_destroy will call mp_free, so rqs_think
	 * would be deallocated through this call. */
	rqsched_common_destroy(&rqs_think->common);
}

void rqsched_pre_request_think(request_t* rq) {
	rqsched_think_t* rqs_think = (rqsched_think_t*) rq->rq_workload->wl_rqsched_private;

	rqsched_pre_request_iat(rq);

	rq->rq_user_id = rg_generate_int(rqs_think->rqs_user_randgen) % rqs_think->rqs_nusers;
}

void rqsched_post_request_think(request_t* rq) {
	ts_time_t rq_time = tm_diff(rq->rq_start_time, rq->rq_end_time);
	thread_pool_t* tp = rq->rq_workload->wl_tp;
	request_t* next_rq = rq->rq_wl_next;

	while(next_rq != NULL) {
		if(next_rq->rq_user_id == rq->rq_user_id) {
			next_rq->rq_sched_time += rq_time;
			tp->tp_disp->tpd_class->relink_request(tp, next_rq);
		}

		next_rq = next_rq->rq_wl_next;
	}
}

rqsched_class_t think_rqsched_class = {
	RQSCHED_NAME("think"),

	SM_INIT(.rqsched_fini, rqsched_fini_think),
	SM_INIT(.rqsched_step, rqsched_step_iat),
	SM_INIT(.rqsched_pre_request, rqsched_pre_request_think),
	SM_INIT(.rqsched_post_request, rqsched_post_request_think)
};