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

extern tp_disp_class_t tpd_rr_class;
extern tp_disp_class_t tpd_rand_class;
extern tp_disp_class_t tpd_fill_up_class;
extern tp_disp_class_t tpd_ff_class;

int tpd_preinit_fill_up(tp_disp_t* tpd, unsigned num_requests, int first_wid);

boolean_t tpd_wait_for_arrival(request_t* rq, ts_time_t max_sleep) {
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
request_t* tpd_wqueue_pick(thread_pool_t* tp, tp_worker_t* worker) {
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

void tpd_wqueue_done(thread_pool_t* tp, tp_worker_t* worker, request_t* rq) {
	mutex_lock(&worker->w_rq_mutex);
	list_del(&rq->rq_w_node);
	mutex_unlock(&worker->w_rq_mutex);
}

/**
 * Put single request onto queue of worker and wake up worker
 */
void tpd_wqueue_put(thread_pool_t* tp, tp_worker_t* worker, request_t* rq) {
	mutex_lock(&worker->w_rq_mutex);

	list_add_tail(&rq->rq_w_node, &worker->w_rq_head);
	/* Wake up control thread if it is reporting now */
	cv_notify_one(&worker->w_rq_cv);

	mutex_unlock(&worker->w_rq_mutex);
}

void tpd_wqueue_signal(thread_pool_t* tp, int wid) {
	tp_worker_t* worker = tp->tp_workers + wid;

	mutex_lock(&worker->w_rq_mutex);
	cv_notify_one(&worker->w_rq_cv);
	mutex_unlock(&worker->w_rq_mutex);
}

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
