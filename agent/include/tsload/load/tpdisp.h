#ifndef TPDISP_H
#define TPDISP_H

#include <tsload/defs.h>
#include <tsload/modules.h>
#include <tsload/autostring.h>

#include <tsload/obj/obj.h>

#include <tsload/load/workload.h>
#include <tsload/load/threadpool.h>

#include <stdlib.h>


struct tp_disp;
struct tsload_param;

/**
 * @module ThreadPool Dispatchers
 *
 * ThreadPool dispatcher is set of hooks that simulate request arrivals,
 * distributes requests between workers and reports finished requests.
 *
 * Hooks (see `tp_disp_class`):
 *  * preinit - actually called by json_tp_disp_proc factory to set params
 * 	* init/destroy - initialize/destroy private tpd data
 *  * proc_tsobj - process TSObject to set tpd parameter. May be set to NULL.
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

#define TPDHASHSIZE			8
#define TPDHASHMASK			(TPDHASHSIZE - 1)

/**
 * ThreadPool dispatcher error code
 */
#define TPD_OK				0
#define TPD_ERROR			-1
#define TPD_BAD				-2

typedef struct tp_disp_class {
	AUTOSTRING char* name;
	const char* description;
	struct tsload_param* params;

	int (*init)(thread_pool_t* tp);
	void (*destroy)(thread_pool_t* tp);
	int (*proc_tsobj)(struct tp_disp* tpd, tsobj_node_t* node);

	void (*control_report)(thread_pool_t* tp);
	void (*control_sleep)(thread_pool_t* tp);

	request_t* (*worker_pick)(thread_pool_t* tp, tp_worker_t* worker);
	void (*worker_done)(thread_pool_t* tp, tp_worker_t* worker, request_t* rq);
	void (*worker_signal)(thread_pool_t* tp, int wid);

	void (*relink_request)(thread_pool_t* tp, request_t* rq);
	
	struct tp_disp_class* next;
	module_t* mod;
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

tsobj_node_t* tsobj_tpd_class_format(tp_disp_class_t* tpd_class);
TESTEXPORT tp_disp_t* tsobj_tp_disp_proc(tsobj_node_t* node);

LIBEXPORT void tpd_destroy(tp_disp_t* tpd);

LIBEXPORT int tpdisp_register(module_t* mod, tp_disp_class_t* class);
LIBEXPORT int tpdisp_unregister(module_t* mod, tp_disp_class_t* class);

LIBEXPORT int tpdisp_init(void);
LIBEXPORT void tpdisp_fini(void);

#endif
