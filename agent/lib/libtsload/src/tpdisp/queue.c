/*
 * queue.c
 *
 *  Created on: Jan 25, 2014
 *      Author: myaut
 */

#define LOG_SOURCE "tpd-queue"
#include <log.h>

#include <tpdisp.h>
#include <threadpool.h>
#include <threads.h>
#include <mempool.h>
#include <tstime.h>
#include <workload.h>
#include <list.h>
#include <errcode.h>
#include <tsload.h>

#include <assert.h>

/**
 * #### Queue-based dispatcher classes
 *
 * Starting each step, pre-distributes requests among workers then sleeps
 * Has four options:
 * 		- Round-robin
 * 		- Random
 * 		- Fill up to N requests
 * 		- Per-user
 * */

int tpd_init_queue(thread_pool_t* tp) {
	return TPD_OK;
}

void tpd_destroy_queue(thread_pool_t* tp) {
	/* NOTHING */
}

int tpd_get_queue_len_queue(thread_pool_t* tp, tp_worker_t* worker, request_t* rq) {
	int qlen = 0;
	ts_time_t cur_time = tm_get_clock();
	ts_time_t next_time;
	request_t* rq_next;

	mutex_lock(&worker->w_rq_mutex);

	list_for_each_entry(request_t, rq_next, &worker->w_rq_head, rq_w_node) {
		next_time = rq_next->rq_sched_time + rq->rq_workload->wl_start_clock;

		if(next_time > cur_time)
			break;

		++qlen;
	}

	mutex_unlock(&worker->w_rq_mutex);

	return qlen;
}

request_t* tpd_worker_pick_queue(thread_pool_t* tp, tp_worker_t* worker) {
	request_t* rq = tpd_wqueue_pick(tp, worker);

	if(rq != NULL) {
		rq->rq_queue_len = tpd_get_queue_len_queue(tp, worker, rq);

		tpd_wait_for_arrival(rq, TS_TIME_MAX);
	}

	return rq;
}

void tpd_worker_done_queue(thread_pool_t* tp, tp_worker_t* worker, request_t* rq) {
	mutex_lock(&worker->w_rq_mutex);
	list_del(&rq->rq_w_node);
	cv_notify_one(&worker->w_rq_cv);
	mutex_unlock(&worker->w_rq_mutex);
}

struct tpd_queue_rq_nodes {
	list_node_t* prev_rq_node;
	list_node_t* next_rq_node;
};

void tpd_control_sleep_queue(thread_pool_t* tp, int wid,
							 int (*next_wid)(thread_pool_t* tp, int wid, request_t* rq)) {
	request_t* rq;
	int lwid = 0;
	tp_worker_t* worker;
	ts_time_t cur_time;

	struct tpd_queue_rq_nodes* worker_nodes =
			mp_malloc(tp->tp_num_threads * sizeof(struct tpd_queue_rq_nodes));

	for(lwid = 0; lwid < tp->tp_num_threads; ++lwid) {
		worker = tp->tp_workers + lwid;

		mutex_lock(&worker->w_rq_mutex);

		tp_insert_request_initnodes(&worker->w_rq_head,
								    &worker_nodes[lwid].prev_rq_node,
								    &worker_nodes[lwid].next_rq_node);
	}

	list_for_each_entry(request_t, rq, &tp->tp_rq_head, rq_node) {
		/* Requests left from previous steps - do not touch them */
		if(rq->rq_step != rq->rq_workload->wl_current_step)
			continue;

		wid = next_wid(tp, wid, rq);

		rq->rq_thread_id = wid;
		worker = tp->tp_workers + wid;

		/* With think-time request scheduler, some requests may be left
		 * because step is ended and they can not discarded (due to tp policy).
		 * In that case they may be first in queue but not first by
		 * schedule time, because current step arrivals come earlier. So
		 * fall back to slower tp_insert_request() that guarantees
		 * ordered requests queue */
		if( tp->tp_discard ||
				(worker_nodes[wid].prev_rq_node == NULL &&
				 worker_nodes[wid].next_rq_node == NULL)) {
			list_add_tail(&rq->rq_w_node, &worker->w_rq_head);
		}
		else {
			tp_insert_request(&worker->w_rq_head, &rq->rq_w_node,
							  &worker_nodes[wid].prev_rq_node,
							  &worker_nodes[wid].next_rq_node, rq_w_node);
		}
	}

	for(lwid = 0; lwid < tp->tp_num_threads; ++lwid) {
		worker = tp->tp_workers + lwid;

		cv_notify_one(&worker->w_rq_cv);
		mutex_unlock(&worker->w_rq_mutex);
	}

	mp_free(worker_nodes);

	cur_time = tm_get_clock();
	if(cur_time < tp->tp_time + tp->tp_quantum)
			tm_sleep_nano(tm_diff(cur_time, tp->tp_time + tp->tp_quantum));
}

void tpd_control_report_queue(thread_pool_t* tp) {
	int wid = 0;
	tp_worker_t* worker;
	request_t* rq;

	list_head_t* rq_list = (list_head_t*) mp_malloc(sizeof(list_head_t));

	list_head_init(rq_list, "rqs-%s-out", tp->tp_name);
	list_splice_init(&tp->tp_rq_head, list_head_node(rq_list));

	for(wid = 0; wid < tp->tp_num_threads; ++wid) {
		worker = tp->tp_workers + wid;

		mutex_lock(&worker->w_rq_mutex);

		if(tp->tp_discard) {
			/* Discard unfinished requests - simply reinitialize worker's queue */
			list_head_reset(&worker->w_rq_head);
		}
		else {
			/* There is at least one request on worker's queue, let it finish
			 * current request. At first look this may look like race between
			 * tpd_control_report_queue() and tpd_wqueue_pick(), but really
			 * tpd_wqueue_pick() will get request after current, so it would
			 * be executed while we are reporting requests that already executed:
			 *
			 * CTL                 WORK
			 * cv_wait()
			 * 					   tpd_wqueue_pick()
			 * 					   cv_notify_one()
			 * lock w_rq_mutex ->
			 * ...                 (run first request
			 * ...                 from w_rq_head)
			 * ...                 unlock w_rq_mutex()
			 * lock w_rq_mutex <-
			 * (report requests
			 *  until w_rq_head->next) */
			if(!list_empty(&worker->w_rq_head)) {
				/* Return requests that not yet finished back to TP queue */
				rq = list_first_entry(request_t, &worker->w_rq_head, rq_w_node);
				list_for_each_entry_from(request_t, rq, &worker->w_rq_head, rq_w_node) {
					list_del(&rq->rq_node);
					list_add(&rq->rq_node, &tp->tp_rq_head);
				}
			}
		}

		mutex_unlock(&worker->w_rq_mutex);
	}

	wl_report_requests(rq_list);
}

void tpd_relink_request_queue(thread_pool_t* tp, request_t* rq) {
	tp_worker_t* worker = NULL;
	request_t* prev_rq = NULL;
	request_t* next_rq = NULL;

	list_node_t* prev_rq_node = NULL;
	list_node_t* next_rq_node = NULL;

	boolean_t need_relink = B_FALSE;

	if(list_node_alone(&rq->rq_w_node)) {
		/* Request not yet linked - concurrency between relink and tpd_control_sleep_queue().
		 * So tpd_control_sleep_queue() will link request correctly - abandon it. */
		return;
	}

	assert((rq->rq_thread_id >= 0) && (rq->rq_thread_id < tp->tp_num_threads));

	worker = tp->tp_workers + rq->rq_thread_id;

	mutex_lock(&worker->w_rq_mutex);

	if(!list_is_first(&rq->rq_w_node, &worker->w_rq_head)) {
		prev_rq = list_prev_entry(request_t, rq, rq_w_node);
		prev_rq_node = &prev_rq->rq_w_node;

		if(tp_compare_requests(rq, prev_rq) >= 0) {
			need_relink = B_TRUE;
		}
	}

	if(!list_is_last(&rq->rq_w_node, &worker->w_rq_head)) {
		next_rq = list_next_entry(request_t, rq, rq_w_node);
		next_rq_node = &next_rq->rq_w_node;

		if(tp_compare_requests(rq, next_rq) < 0) {
			need_relink = B_TRUE;
		}
	}

	if(need_relink) {
		list_del(&rq->rq_w_node);
		tp_insert_request(&worker->w_rq_head, &rq->rq_w_node, &prev_rq_node, &next_rq_node, rq_w_node);
	}

	mutex_unlock(&worker->w_rq_mutex);
}

void tpd_control_sleep_rr(thread_pool_t* tp) {
	int wid = tpd_first_wid_rand(tp);

	tpd_control_sleep_queue(tp, wid, tpd_next_wid_rr);
}

tp_disp_class_t tpd_rr_class = {
	tpd_init_queue,
	tpd_destroy_queue,
	tpd_control_report_queue,
	tpd_control_sleep_rr,
	tpd_worker_pick_queue,
	tpd_worker_done_queue,
	tpd_wqueue_signal,
	tpd_relink_request_queue
};

void tpd_control_sleep_rand(thread_pool_t* tp) {
	int wid = tpd_first_wid_rand(tp);

	tpd_control_sleep_queue(tp, wid, tpd_next_wid_rand);
}

tp_disp_class_t tpd_rand_class = {
	tpd_init_queue,
	tpd_destroy_queue,
	tpd_control_report_queue,
	tpd_control_sleep_rand,
	tpd_worker_pick_queue,
	tpd_worker_done_queue,
	tpd_wqueue_signal,
	tpd_relink_request_queue
};

static int tpd_next_wid_user(thread_pool_t* tp, int wid, request_t* rq) {
	return rq->rq_user_id % tp->tp_num_threads;
}

void tpd_control_sleep_user(thread_pool_t* tp) {
	tpd_control_sleep_queue(tp, 0, tpd_next_wid_user);
}

tp_disp_class_t tpd_user_class = {
	tpd_init_queue,
	tpd_destroy_queue,
	tpd_control_report_queue,
	tpd_control_sleep_user,
	tpd_worker_pick_queue,
	tpd_worker_done_queue,
	tpd_wqueue_signal,
	tpd_relink_request_queue
};

typedef struct {
	unsigned fill_requests;
	unsigned num_rqs;
	int 	 first_wid;
	int 	 wid;
} tpd_fill_up_t;

int tpd_preinit_fill_up(tp_disp_t* tpd, unsigned num_requests, int first_wid) {
	tpd_fill_up_t* fill_up = mp_malloc(sizeof(tpd_fill_up_t));

	fill_up->num_rqs = 0;
	fill_up->fill_requests = num_requests;
	fill_up->first_wid = fill_up->wid = first_wid;

	tpd->tpd_data = fill_up;

	return TPD_OK;
}

int tpd_init_fill_up(thread_pool_t* tp) {
	tpd_fill_up_t* fill_up = (tpd_fill_up_t*) tp->tp_disp->tpd_data;

	if(fill_up->first_wid >= tp->tp_num_threads) {
		tsload_error_msg(TSE_INVALID_DATA,
						 "Fill-Up dispatcher: Worker id #%d is too large for tp '%s'",
						 fill_up->first_wid, tp->tp_name);
		return TPD_ERROR;
	}

	return TPD_OK;
}

static int tpd_next_wid_fill_up(thread_pool_t* tp, int wid, request_t* rq) {
	tpd_fill_up_t* fill_up = (tpd_fill_up_t*) tp->tp_disp->tpd_data;

	++fill_up->num_rqs;

	if(fill_up->num_rqs == fill_up->fill_requests) {
		fill_up->num_rqs = 0;
		++fill_up->wid;

		if(fill_up->wid == tp->tp_num_threads) {
			fill_up->wid = 0;
		}
	}

	return fill_up->wid;
}

void tpd_control_sleep_fill_up(thread_pool_t* tp) {
	tpd_fill_up_t* fill_up = (tpd_fill_up_t*) tp->tp_disp->tpd_data;

	fill_up->wid = fill_up->first_wid;
	fill_up->num_rqs = 0;

	tpd_control_sleep_queue(tp, fill_up->first_wid, tpd_next_wid_fill_up);
}

tp_disp_class_t tpd_fill_up_class = {
	tpd_init_fill_up,
	tpd_destroy_queue,
	tpd_control_report_queue,
	tpd_control_sleep_fill_up,
	tpd_worker_pick_queue,
	tpd_worker_done_queue,
	tpd_wqueue_signal,
	tpd_relink_request_queue
};
