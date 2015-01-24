
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

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

#include <tsload/obj/obj.h>

#include <tsload/mempool.h>
#include <tsload/time.h>

#include <tsload/load/randgen.h>
#include <tsload/load/rqsched.h>
#include <tsload/load/tpdisp.h>
#include <tsload/load/threadpool.h>
#include <tsload.h>

#include <errormsg.h>

#include <math.h>


/**
 * #### Think-time based request scheduler
 *  */

extern void rqsched_pre_request_iat(request_t* rq);
extern void rqsched_step_iat(workload_step_t* step);

typedef struct rqsched_think {
	int nusers;
	randgen_t* user_randgen;
} rqsched_think_t;

int tsobj_rqsched_proc_think(tsobj_node_t* node, workload_t* wl, rqsched_t* rqs) {
	rqsched_think_t* rqs_think = mp_malloc(sizeof(rqsched_think_t));
	int ret;
	
	if(tsobj_get_integer_i(node, "nusers", &rqs_think->nusers) != TSOBJ_OK) {
		ret = RQSCHED_TSOBJ_BAD;
		goto error;
	}

	if(rqs_think->nusers < 1) {
		tsload_error_msg(TSE_INVALID_VALUE, RQSCHED_ERROR_PREFIX "number of users %d must be 1 or greater",
						 wl->wl_name, rqs_think->nusers);		
		ret = RQSCHED_TSOBJ_ERROR;
		goto error;
	}

	ret = tsobj_rqsched_proc_randgen(node, "user_randgen", &rqs_think->user_randgen);
	if(ret != RQSCHED_TSOBJ_OK)
		goto error;
	
	rqs->rqs_private = rqs_think;
	return RQSCHED_TSOBJ_OK;
	
error:
	mp_free(rqs_think);
	return ret;
}

void rqsched_fini_think(workload_t* wl) {
	rqsched_t* rqs = (rqsched_t*) wl->wl_rqsched_private;
	rqsched_think_t* rqs_think = (rqsched_think_t*) rqs->rqs_private;

	rg_destroy(rqs_think->user_randgen);
	
	mp_free(rqs_think);
}

void rqsched_pre_request_think(request_t* rq) {
	rqsched_t* rqs = (rqsched_t*) rq->rq_workload->wl_rqsched_private;
	rqsched_think_t* rqs_think = (rqsched_think_t*) rqs->rqs_private;
	
	rqsched_pre_request_iat(rq);

	rq->rq_user_id = rg_generate_int(rqs_think->user_randgen) % rqs_think->nusers;
}

void rqsched_post_request_think(request_t* rq) {
	ts_time_t rq_time = tm_diff(rq->rq_start_time, rq->rq_end_time);
	thread_pool_t* tp = rq->rq_workload->wl_tp;
	workload_t* wl = rq->rq_workload;
	request_t* next_rq = NULL;

	mutex_lock(&wl->wl_rq_mutex);

	if(!list_is_last(&rq->rq_wl_node, &wl->wl_requests)) {
		next_rq = list_next_entry(request_t, rq, rq_wl_node);

		list_for_each_entry_continue(request_t, next_rq, &wl->wl_requests, rq_wl_node) {
			if(next_rq->rq_user_id == rq->rq_user_id) {
				next_rq->rq_sched_time += rq_time;
				tp->tp_disp->tpd_class->relink_request(tp, next_rq);
			}
		}
	}

	mutex_unlock(&wl->wl_rq_mutex);
}

rqsched_class_t rqsched_think_class = {
	RQSCHED_NAME("think"),
	RQSCHED_NEED_VARIATOR,
		
	SM_INIT(.rqsched_proc_tsobj, tsobj_rqsched_proc_think),
	SM_INIT(.rqsched_fini, rqsched_fini_think),
	SM_INIT(.rqsched_step, rqsched_step_iat),
	SM_INIT(.rqsched_pre_request, rqsched_pre_request_think),
	SM_INIT(.rqsched_post_request, rqsched_post_request_think)
};

