
/*
    This file is part of TSLoad.
    Copyright 2012-2014, Sergey Klyaus, ITMO University

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

#include <tsload/load/rqsched.h>
#include <tsload.h>

void rqsched_fini_simple(workload_t* wl, rqsched_t* rqs) {
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

tsload_param_t rqsched_simple_params[] = {
	{ TSLOAD_PARAM_NULL, NULL, NULL }
};

rqsched_class_t rqsched_simple_class = {
	RQSCHED_NAME("simple"),
	"Sets arrival time for all requests to zero, so it "
	"forces threadpool to execute request ASAP.",
	
	RQSCHED_NO_FLAGS,
	rqsched_simple_params,

	SM_INIT(.rqsched_proc_tsobj, NULL),
	SM_INIT(.rqsched_fini, rqsched_fini_simple),
	SM_INIT(.rqsched_step, rqsched_step_simple),
	SM_INIT(.rqsched_pre_request, rqsched_pre_request_simple),
	SM_INIT(.rqsched_post_request, rqsched_post_request_simple)
};

