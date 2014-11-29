
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

#include <tsload/mempool.h>
#include <tsload/time.h>
#include <tsload/list.h>

#include <tsload/load/tpdisp.h>
#include <tsload/load/threadpool.h>
#include <tsload/load/workload.h>

#include <assert.h>


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

		assert(list_empty(&worker->w_rq_head));
	}

	assert(list_empty(&ff->ff_finished));
	assert(ff->ff_last_rq == NULL);

	mutex_destroy(&ff->ff_mutex);
	cv_destroy(&ff->ff_control_cv);

	mp_free(ff);
}

int tpd_get_queue_len_ff(thread_pool_t* tp, request_t* rq) {
	int qlen = 0;
	ts_time_t cur_time = tm_get_clock();
	ts_time_t next_time;
	request_t* rq_next;

	list_for_each_entry(request_t, rq_next, &tp->tp_rq_head, rq_node) {
		next_time = rq_next->rq_sched_time + rq->rq_workload->wl_start_clock;

		if(next_time > cur_time)
			break;

		++qlen;
	}

	return qlen;
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

	mutex_lock(&ff->ff_mutex);

	list_for_each_entry_safe(request_t, rq, rq_next, &tp->tp_rq_head, rq_node) {
		dispatched = B_FALSE;
		rq->rq_flags |= RQF_DISPATCHED;
		if(!list_is_last(&rq->rq_node, &tp->tp_rq_head)) {
			rq_next->rq_flags |= RQF_DISPATCHED;
		}

		mutex_unlock(&ff->ff_mutex);

		if(!tpd_wait_for_arrival(rq, tp->tp_time + tp->tp_quantum)) {
			return;
		}

		mutex_lock(&ff->ff_mutex);

		rq->rq_queue_len = tpd_get_queue_len_ff(tp, rq);

		list_del(&rq->rq_node);

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

			wid = tpd_next_wid_rr(tp, wid, rq);
		}

		if(!dispatched) {
			while(ff->ff_last_rq != NULL) {
				cur_time = tm_get_clock();
				if(cur_time > (tp->tp_time + tp->tp_quantum)) {
					/* Oops - we didn't managed to dispatch request.
					 * Return it back to rq_head, so no one will notice*/
					list_add(&rq->rq_node, &tp->tp_rq_head);
					mutex_unlock(&ff->ff_mutex);
					return;
				}

				max_sleep = tm_diff(cur_time, tp->tp_time + tp->tp_quantum);
				cv_wait_timed(&ff->ff_control_cv, &ff->ff_mutex, max_sleep);
			}

			ff->ff_last_rq = rq;
		}
	}

	mutex_unlock(&ff->ff_mutex);

	cur_time = tm_get_clock();
	if(cur_time < (tp->tp_time + tp->tp_quantum)) {
		max_sleep = tm_diff(cur_time, tp->tp_time + tp->tp_quantum);
		tm_sleep_nano(max_sleep);
	}
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

		/* If some requests was dispatched, but not yet handled by workers,
		 * discard them too. */
		if(ff->ff_last_rq != NULL)
			list_add_tail(&ff->ff_last_rq->rq_node, rq_list);

		for(wid = 0; wid < tp->tp_num_threads; ++wid) {
			worker = tp->tp_workers + wid;

			mutex_lock(&worker->w_rq_mutex);
			if(!list_empty(&worker->w_rq_head)) {
				rq = list_first_entry(request_t, &worker->w_rq_head, rq_w_node);

				if(!(rq->rq_flags & RQF_DEQUEUED)) {
					list_del(&rq->rq_w_node);
					list_add_tail(&rq->rq_node, rq_list);
				}
			}
			mutex_unlock(&worker->w_rq_mutex);
		}
	}
	else {
		list_splice_init(&ff->ff_finished, list_head_node(rq_list));

		if(ff->ff_last_rq != NULL)
			list_add(&ff->ff_last_rq->rq_node, &tp->tp_rq_head);
	}

	ff->ff_last_rq = NULL;

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

		cv_notify_one(&ff->ff_control_cv);
	}
	else {
		*wstate = FF_WSTATE_SLEEPING;
	}

	mutex_unlock(&ff->ff_mutex);

	if(rq == NULL) {
		rq = tpd_wqueue_pick(tp, worker);
	}
	if(rq != NULL) {
		rq->rq_thread_id = worker->w_thread.t_local_id;
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

void tpd_relink_request_ff(thread_pool_t* tp, request_t* rq) {
	tpd_ff_t* ff = (tpd_ff_t*) tp->tp_disp->tpd_data;
	request_t* prev_rq = NULL;
	request_t* next_rq = NULL;

	list_node_t* prev_rq_node = NULL;
	list_node_t* next_rq_node = NULL;

	boolean_t need_relink = B_FALSE;

	mutex_lock(&ff->ff_mutex);

	if(rq->rq_flags & RQF_DISPATCHED) {
		/* Already in control thread -- oops */
		mutex_unlock(&ff->ff_mutex);
		return;
	}

	prev_rq = list_prev_entry(request_t, rq, rq_node);
	prev_rq_node = &prev_rq->rq_node;

	if(tp_compare_requests(rq, prev_rq) >= 0) {
		need_relink = B_TRUE;
	}

	if(!list_is_last(&rq->rq_w_node, &tp->tp_rq_head)) {
		next_rq = list_next_entry(request_t, rq, rq_node);
		next_rq_node = &next_rq->rq_node;

		if(tp_compare_requests(rq, next_rq) < 0) {
			need_relink = B_TRUE;
		}
	}

	if(need_relink) {
		list_del(&rq->rq_node);
		tp_insert_request(&tp->tp_rq_head, &rq->rq_node, &prev_rq_node, &next_rq_node, rq_node);
	}

	mutex_unlock(&ff->ff_mutex);
}

tp_disp_class_t tpd_ff_class = {
	"first-free",

	tpd_init_ff,
	tpd_destroy_ff,
	tpd_control_report_ff,
	tpd_control_sleep_ff,
	tpd_worker_pick_ff,
	tpd_worker_done_ff,
	tpd_wqueue_signal,
	tpd_relink_request_ff
};

