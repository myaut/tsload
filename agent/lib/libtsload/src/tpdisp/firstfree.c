/*
 * firstfree.c
 *
 *  Created on: Jan 25, 2014
 *      Author: myaut
 */

#include <tpdisp.h>
#include <threadpool.h>
#include <threads.h>
#include <mempool.h>
#include <tstime.h>
#include <workload.h>
#include <list.h>

/**
 * #### First-Free thread pool dispatcher
 *
 * Dispatches request to first worker that finishes it's request or
 * to random worker that sleeping.
 */

typedef struct {
	thread_mutex_t ff_mutex;
	thread_cv_t ff_control_cv;
	request_t* ff_last_rq;
	list_head_t ff_finished;
} tpd_ff_t;

typedef enum {
	FF_WSTATE_SLEEPING,
	FF_WSTATE_WORKING
} tpd_ff_wstate_t;

int tpd_init_ff(thread_pool_t* tp) {
	tpd_ff_t* ff = mp_malloc(sizeof(tpd_ff_t));
	tpd_ff_wstate_t* wstate;
	int wid;
	tp_worker_t* worker;

	mutex_init(&ff->ff_mutex, "ff-mutex");
	cv_init(&ff->ff_control_cv, "ff-cv");
	list_head_init(&ff->ff_finished, "ff-finished");

	ff->ff_last_rq = NULL;

	tp->tp_disp->tpd_data = ff;

	for(wid = 0; wid < tp->tp_num_threads; ++wid) {
		worker = tp->tp_workers + wid;
		wstate = mp_malloc(sizeof(tpd_ff_wstate_t));

		*wstate = FF_WSTATE_SLEEPING;

		worker->w_tpd_data = wstate;
	}

	return TPD_OK;
}

void tpd_destroy_ff(thread_pool_t* tp) {
	tpd_ff_t* ff = (tpd_ff_t*) tp->tp_disp->tpd_data;
	int wid;
	tp_worker_t* worker;

	for(wid = 0; wid < tp->tp_num_threads; ++wid) {
		worker = tp->tp_workers + wid;
		mp_free(worker->w_tpd_data);
	}

	mutex_destroy(&ff->ff_mutex);
	cv_destroy(&ff->ff_control_cv);

	mp_free(ff);
}

void tpd_control_sleep_ff(thread_pool_t* tp) {
	tpd_ff_t* ff = (tpd_ff_t*) tp->tp_disp->tpd_data;
	tpd_ff_wstate_t* wstate;
	tp_worker_t* worker;

	request_t* rq;
	request_t* rq_next;

	int wid, i;
	boolean_t dispatched;

	ts_time_t max_sleep, sleep_time;
	ts_time_t cur_time = tm_get_clock();

	list_for_each_entry_safe(request_t, rq, rq_next, &tp->tp_rq_head, rq_node) {
		dispatched = B_FALSE;

		max_sleep = tm_diff(cur_time, tp->tp_time + tp->tp_quantum);
		if(!tpd_wait_for_arrival(rq, max_sleep)) {
			return;
		}

		list_del(&rq->rq_node);

		mutex_lock(&ff->ff_mutex);

		/* Try to dispatch request `rq` to one of of workers (selected randomly).
		 * If it is not busy, put it onto worker queue.
		 * If worker is busy, try next worker.
		 * If all workers are busy, then put request to mailbox and sleep.
		 */
		wid = tpd_first_wid_rand(tp);
		for(i = 0; i < tp->tp_num_threads; ++i) {
			worker = tp->tp_workers + wid;
			wstate = (tpd_ff_wstate_t*) worker->w_tpd_data;

			if(*wstate == FF_WSTATE_SLEEPING) {
				*wstate = FF_WSTATE_WORKING;
				tpd_wqueue_put(tp, worker, rq);
				dispatched = B_TRUE;
				break;
			}

			wid = tpd_next_wid_rr(tp, wid);
		}

		if(!dispatched) {
			ff->ff_last_rq = rq;
			cv_wait_timed(&ff->ff_control_cv, &ff->ff_mutex, max_sleep);
		}

		mutex_unlock(&ff->ff_mutex);

		cur_time = tm_get_clock();
	}

	if(cur_time < tp->tp_time + tp->tp_quantum)
		tm_sleep_nano(tm_diff(cur_time, tp->tp_time + tp->tp_quantum));
}

void tpd_control_report_ff(thread_pool_t* tp) {
	tpd_ff_t* ff = (tpd_ff_t*) tp->tp_disp->tpd_data;
	list_head_t* rq_list = (list_head_t*) mp_malloc(sizeof(list_head_t));
	request_t* rq;
	list_node_t* pos;

	int wid;
	tpd_ff_wstate_t* wstate;
	tp_worker_t* worker;

	list_head_init(rq_list, "rqs-%s-out", tp->tp_name);

	mutex_lock(&ff->ff_mutex);
	if(tp->tp_discard) {
		list_merge(rq_list, &ff->ff_finished, &tp->tp_rq_head);
	}
	else {
		list_splice_init(&ff->ff_finished, list_head_node(rq_list));
	}
	mutex_unlock(&ff->ff_mutex);

	wl_report_requests(rq_list);
}

request_t* tpd_worker_pick_ff(thread_pool_t* tp, tp_worker_t* worker) {
	tpd_ff_wstate_t* wstate = (tpd_ff_wstate_t*) worker->w_tpd_data;
	tpd_ff_t* ff = (tpd_ff_t*) tp->tp_disp->tpd_data;
	request_t* rq = NULL;

	mutex_lock(&ff->ff_mutex);

	if(ff->ff_last_rq != NULL) {
		rq = ff->ff_last_rq;
		ff->ff_last_rq = NULL;
	}
	else {
		*wstate = FF_WSTATE_SLEEPING;
	}

	cv_notify_one(&ff->ff_control_cv);
	mutex_unlock(&ff->ff_mutex);

	if(rq == NULL) {
		rq = tpd_wqueue_pick(tp, worker);
	}

	return rq;
}

void tpd_worker_done_ff(thread_pool_t* tp, tp_worker_t* worker, request_t* rq) {
	tpd_ff_wstate_t* wstate = (tpd_ff_wstate_t*) worker->w_tpd_data;
	tpd_ff_t* ff = (tpd_ff_t*) tp->tp_disp->tpd_data;

	/* rq was taken from wqueue - put it back */
	if(!list_node_alone(&rq->rq_w_node))
		tpd_wqueue_done(tp, worker, rq);

	mutex_lock(&ff->ff_mutex);
	list_add_tail(&rq->rq_node, &ff->ff_finished);
	mutex_unlock(&ff->ff_mutex);
}

tp_disp_class_t tpd_ff_class = {
	tpd_init_ff,
	tpd_destroy_ff,
	tpd_control_report_ff,
	tpd_control_sleep_ff,
	tpd_worker_pick_ff,
	tpd_worker_done_ff,
	tpd_wqueue_signal
};
