#ifndef TPDISP_H
#define TPDISP_H

#include <workload.h>
#include <threadpool.h>

struct tp_disp;

#define TPD_OK				0
#define TPD_ERROR			-1

typedef struct tp_disp_class {
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

#ifndef NO_JSON
#include <libjson.h>

LIBEXPORT tp_disp_t* json_tp_disp_proc(JSONNODE* node);
#endif

LIBEXPORT void tpd_destroy(tp_disp_t* tpd);

#endif
