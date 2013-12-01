/*
 * simple.c
 *
 *  Created on: 04.12.2012
 *      Author: myaut
 */

#include <defs.h>

#include <disp.h>

void disp_fini_simple(workload_t* wl) {
	wl->wl_disp_private = NULL;
}

void disp_step_simple(workload_step_t* step) {
	/* NOTHING */
}

void disp_pre_request_simple(request_t* rq) {
	rq->rq_sched_time = 0;
}

void disp_post_request_simple(request_t* rq) {
	/* NOTHING */
}

disp_class_t simple_disp = {
	DISP_NAME("simple"),

	SM_INIT(.disp_fini, disp_fini_simple),
	SM_INIT(.disp_step, disp_step_simple),
	SM_INIT(.disp_pre_request, disp_pre_request_simple),
	SM_INIT(.disp_post_request, disp_post_request_simple)
};
