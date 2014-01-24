/*
 * tpdisp.c
 *
 *  Created on: Jan 23, 2014
 *      Author: myaut
 */

#define LOG_SOURCE "tpool"
#include <log.h>

#include <tpdisp.h>
#include <threads.h>
#include <workload.h>
#include <tstime.h>
#include <mempool.h>
#include <errcode.h>
#include <tsload.h>

#include <libjson.h>

#include <stdlib.h>
#include <string.h>

/**
 * @name ThreadPol Dispatchers
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
 */

static boolean_t tpd_wait_for_arrival(request_t* rq, ts_time_t max_sleep) {
	ts_time_t cur_time, next_time;
	ts_time_t sleep_time;

	cur_time = tm_get_clock();
	next_time = rq->rq_sched_time;
	sleep_time = tm_diff(cur_time, next_time) - TP_WORKER_OVERHEAD;

	if(sleep_time > max_sleep)
		return B_FALSE;

	if(cur_time < next_time && sleep_time > TP_WORKER_MIN_SLEEP) {
		tm_sleep_nano(sleep_time);
	}

	return B_TRUE;
}

/**
 * Wait until somebody put request onto worker's queue than return
 * this request. If threadpool dies, returns NULL.
 */
static request_t* tpd_wqueue_pick(thread_pool_t* tp, tp_worker_t* worker) {
	request_t* rq;

	mutex_lock(&worker->w_rq_mutex);

	while(list_empty(&worker->w_rq_head)) {
		cv_wait(&worker->w_rq_cv, &worker->w_rq_mutex);

		if(tp->tp_is_dead) {
			mutex_unlock(&worker->w_rq_mutex);
			return NULL;
		}
	}

	rq = list_first_entry(request_t, &worker->w_rq_head, rq_w_node);

	mutex_unlock(&worker->w_rq_mutex);

	return rq;
}

static void tpd_wqueue_done(thread_pool_t* tp, tp_worker_t* worker, request_t* rq) {
	mutex_lock(&worker->w_rq_mutex);
	list_del(&rq->rq_w_node);
	mutex_unlock(&worker->w_rq_mutex);
}

/**
 * Put single request onto queue of worker and wake up worker
 */
static void tpd_wqueue_put(thread_pool_t* tp, tp_worker_t* worker, request_t* rq) {
	mutex_lock(&worker->w_rq_mutex);

	list_add_tail(&rq->rq_w_node, &worker->w_rq_head);
	/* Wake up control thread if it is reporting now */
	cv_notify_one(&worker->w_rq_cv);

	mutex_unlock(&worker->w_rq_mutex);
}

/**
 * #### Queue-based dispatcher classes
 *
 * Starting each step, pre-distributes requests among workers then sleeps
 * Has two options:
 * 		- Round-robin
 * 		- Random
 * 		- Fill up to N requests
 * */

int tpd_init_queue(thread_pool_t* tp) {
	return TPD_OK;
}

void tpd_destroy_queue(thread_pool_t* tp) {
	/* NOTHING */
}

request_t* tpd_worker_pick_queue(thread_pool_t* tp, tp_worker_t* worker) {
	request_t* rq = tpd_wqueue_pick(tp, worker);

	if(rq != NULL) {
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

void tpd_worker_signal_queue(thread_pool_t* tp, int wid) {
	tp_worker_t* worker = tp->tp_workers + wid;

	mutex_lock(&worker->w_rq_mutex);
	cv_notify_one(&worker->w_rq_cv);
	mutex_unlock(&worker->w_rq_mutex);
}

void tpd_control_sleep_queue(thread_pool_t* tp, int wid,
							 int (*next_wid)(thread_pool_t* tp, int wid)) {
	request_t* rq;
	int lwid = 0;
	tp_worker_t* worker;
	ts_time_t cur_time;

	for(lwid = 0; lwid < tp->tp_num_threads; ++lwid) {
		mutex_lock(&tp->tp_workers[lwid].w_rq_mutex);
	}

	list_for_each_entry(request_t, rq, &tp->tp_rq_head, rq_node) {
		/* Requests left from previous steps - do not touch them */
		if(rq->rq_step != rq->rq_workload->wl_current_step)
			continue;

		list_add_tail(&rq->rq_w_node, &tp->tp_workers[wid].w_rq_head);
		wid = next_wid(tp, wid);
	}

	for(lwid = 0; lwid < tp->tp_num_threads; ++lwid) {
		worker = tp->tp_workers + lwid;

		cv_notify_one(&worker->w_rq_cv);
		mutex_unlock(&worker->w_rq_mutex);
	}

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
				cv_wait(&worker->w_rq_cv, &worker->w_rq_mutex);

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

static int tpd_next_wid_rr(thread_pool_t* tp, int wid) {
	++wid;
	if(wid == tp->tp_num_threads)
		return 0;

	return wid;
}

static int tpd_first_wid_rand(thread_pool_t* tp) {
	/* FIXME: Should use randgen API */
	return rand() % tp->tp_num_threads;
}

static int tpd_next_wid_rand(thread_pool_t* tp, int wid) {
	return tpd_first_wid_rand(tp);
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
	tpd_worker_signal_queue
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
	tpd_worker_signal_queue
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

static int tpd_next_wid_fill_up(thread_pool_t* tp, int wid) {
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
	tpd_worker_signal_queue
};

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

		if(!tp->tp_discard) {
			list_del(&rq->rq_node);
		}

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

	int wid;
	tpd_ff_wstate_t* wstate;
	tp_worker_t* worker;

	list_head_init(rq_list, "rqs-%s-out", tp->tp_name);

	if(tp->tp_discard) {
		list_splice_init(&tp->tp_rq_head, list_head_node(rq_list));
	}
	else {
		mutex_lock(&ff->ff_mutex);
		list_splice_init(&ff->ff_finished, list_head_node(rq_list));
		mutex_unlock(&ff->ff_mutex);
	}

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
	tpd_worker_signal_queue
};

static int json_tp_disp_proc_fill_up(tp_disp_t* tpd, JSONNODE* node);

tp_disp_t* json_tp_disp_proc(JSONNODE* node) {
	tp_disp_t* tpd = NULL;
	JSONNODE_ITERATOR i_type, i_end;
	char* type;

	i_end = json_end(node);
	i_type = json_find(node, "type");

	if(i_type == i_end || json_type(*i_type) != JSON_STRING) {
		tsload_error_msg(TSE_MESSAGE_FORMAT,
				"Failed to parse dispatcher: missing or invalid parameter 'type'");
	}

	type = json_as_string(*i_type);

	tpd = mp_malloc(sizeof(tp_disp_t));
	tpd->tpd_tp = NULL;
	tpd->tpd_data = NULL;

	if(strcmp(type, "round-robin") == 0) {
		tpd->tpd_class = &tpd_rr_class;
	}
	else if(strcmp(type, "random") == 0) {
		tpd->tpd_class = &tpd_rand_class;
	}
	else if(strcmp(type, "first-free") == 0) {
		tpd->tpd_class = &tpd_ff_class;
	}
	else if(strcmp(type, "fill-up") == 0) {
		tpd->tpd_class = &tpd_fill_up_class;

		if(json_tp_disp_proc_fill_up(tpd, node) != TPD_OK) {
			mp_free(tpd);
			tpd = NULL;
		}
	}
	else {
		tsload_error_msg(TSE_INVALID_DATA,
						"Failed to parse dispatcher: invalid dispatcher type '%s'", type);

		mp_free(tpd);
		tpd = NULL;
	}

	json_free(type);

	return tpd;
}

#define CONFIGURE_TPD_PARAM(i_node, name, type) 						\
	i_node = json_find(node, name);										\
	if(i_node == i_end || json_type(*i_node) != type) {					\
		tsload_error_msg(TSE_INVALID_DATA,								\
					  "Failed to parse fill-up dispatcher: "			\
					  "missing or invalid parameter '%s'\n", name);		\
		return TPD_ERROR;												\
	}

static int json_tp_disp_proc_fill_up(tp_disp_t* tpd, JSONNODE* node) {
	unsigned num_rqs;
	int wid;

	JSONNODE_ITERATOR i_num_rqs, i_wid;
	JSONNODE_ITERATOR i_end;

	i_end = json_end(node);
	CONFIGURE_TPD_PARAM(i_num_rqs, "n", JSON_NUMBER);
	CONFIGURE_TPD_PARAM(i_wid, "wid", JSON_NUMBER);

	num_rqs = json_as_int(*i_num_rqs);
	wid = json_as_int(*i_wid);

	return tpd_preinit_fill_up(tpd, num_rqs, wid);
}

void tpd_destroy(tp_disp_t* tpd) {
	tpd->tpd_class->destroy(tpd->tpd_tp);
}
