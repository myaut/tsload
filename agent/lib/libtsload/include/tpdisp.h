#ifndef TPDISP_H
#define TPDISP_H

#include <workload.h>
#include <threadpool.h>

#include <tsobj.h>

#include <stdlib.h>

struct tp_disp;

/**
 * @module ThreadPool Dispatchers
 *
 * ThreadPool dispatcher is set of hooks that simulate request arrivals,
 * distributes requests between workers and reports finished requests.
 *
 * Hooks (see `tp_disp_class`):
 *  * preinit - actually called by json_tp_disp_proc factory to set params
 * 	* init/destroy - initialize/destroy private tpd data
 * 	* control_report - called when control thread wants to report data
 * 	  When discard policy is set, should clear worker queues and report
 * 	  all requests to reporter thread. If not, should split request list into
 * 	  two and report only finished requests.
 *  * control_sleep - called when control thread finished generating requests
 *    May simulate request arrivals
 *  * worker_pick - called when worker wants to pick next request from queue
 *  * worker_done - called when worker finished executing request
 *  * worker_finish - special hook for tp_destroy() code. If dispatcher uses
 *    external cv's, should wakeup worker because threadpool is dying.
 *  * relink_request is called when request's rq_sched_time changes and it should
 *    be again linked to maintain queue sorted.
 */

/**
 * ThreadPool dispatcher error code
 */
#define TPD_OK				0
#define TPD_ERROR			-1
#define TPD_BAD				-2

typedef struct tp_disp_class {
	const char* name;

	int (*init)(thread_pool_t* tp);
	void (*destroy)(thread_pool_t* tp);

	void (*control_report)(thread_pool_t* tp);
	void (*control_sleep)(thread_pool_t* tp);

	request_t* (*worker_pick)(thread_pool_t* tp, tp_worker_t* worker);
	void (*worker_done)(thread_pool_t* tp, tp_worker_t* worker, request_t* rq);
	void (*worker_signal)(thread_pool_t* tp, int wid);

	void (*relink_request)(thread_pool_t* tp, request_t* rq);
} tp_disp_class_t;

typedef struct tp_disp {
	thread_pool_t*	tpd_tp;
	tp_disp_class_t* tpd_class;
	void* tpd_data;
} tp_disp_t;

boolean_t tpd_wait_for_arrival(request_t* rq, ts_time_t max_sleep);

request_t* tpd_wqueue_pick(thread_pool_t* tp, tp_worker_t* worker);
void tpd_wqueue_done(thread_pool_t* tp, tp_worker_t* worker, request_t* rq);
void tpd_wqueue_put(thread_pool_t* tp, tp_worker_t* worker, request_t* rq);
void tpd_worker_wait(thread_pool_t* tp, int wid);
void tpd_wqueue_signal(thread_pool_t* tp, int wid);

static int tpd_next_wid_rr(thread_pool_t* tp, int wid, request_t* rq) {
	++wid;
	if(wid == tp->tp_num_threads)
		return 0;

	return wid;
}

STATIC_INLINE int tpd_first_wid_rand(thread_pool_t* tp) {
	/* FIXME: Should use randgen API */
	return rand() % tp->tp_num_threads;
}

static int tpd_next_wid_rand(thread_pool_t* tp, int wid, request_t* rq) {
	return tpd_first_wid_rand(tp);
}

#define TPD_ERROR_PREFIX		"Failed to parse dispatcher: "

TESTEXPORT tp_disp_t* tsobj_tp_disp_proc(tsobj_node_t* node);

LIBEXPORT void tpd_destroy(tp_disp_t* tpd);

#endif
