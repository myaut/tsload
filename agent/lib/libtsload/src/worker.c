/*
 * worker.c
 *
 *  Created on: 04.12.2012
 *      Author: myaut
 */

#define LOG_SOURCE "worker"
#include <log.h>

#include <defs.h>

#include <mempool.h>
#include <workload.h>
#include <tstime.h>
#include <threads.h>
#include <threadpool.h>
#include <tpdisp.h>
#include <schedutil.h>

#include <assert.h>

static void control_prepare_step(thread_pool_t* tp, workload_t* step);

/**
 * Control thread
 *
 * Control thread notifies workers after each quantum ending
 * and processes each step for each workload on thread pool
 * */
thread_result_t control_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, thread_pool_t, tp);
	ts_time_t tm = tm_get_time();
	workload_t *wl;
	int wl_count = 0, wl_count_prev = 0, wli = 0;
	int wid = 0;

	int wi;
	tp_worker_t* worker;

	tp_hold(tp);

	/* Synchronize all control-threads on all nodes to start
	 * quantum at beginning of next second (real time synchronization
	 * is provided by NTP) */
	tm_sleep_nano(tm_ceil_diff(tm, T_SEC));

	logmsg(LOG_DEBUG, "Started control thread (tpool: %s)", tp->tp_name);

	tp->tp_time = tm_get_clock();

	while(!tp->tp_is_dead) {
		logmsg(LOG_TRACE, "Threadpool '%s': control thread is running (tm: %lld)",
					tp->tp_name, tp->tp_time);

		mutex_lock(&tp->tp_mutex);
		wl_count = tp->tp_wl_count;


		wl_count_prev = wl_count;

		tp->tp_disp->tpd_class->control_report(tp);

		/* Advance step for each workload, then
		 * distribute requests across workers*/
		list_for_each_entry(workload_t, wl, &tp->tp_wl_head, wl_tp_node) {
			control_prepare_step(tp, wl);
		}

		mutex_unlock(&tp->tp_mutex);

		tp->tp_disp->tpd_class->control_sleep(tp);
		tp->tp_time += tp->tp_quantum;
	}

THREAD_END:
	tp_rele(tp, B_FALSE);
	THREAD_FINISH(arg);
}

static void control_prepare_step(thread_pool_t* tp, workload_t* wl) {
	workload_step_t* step;
	workload_t* wl_chain = wl;

	if(unlikely(!wl_is_started(wl))) {
		return;
	}

	if(wl->wl_start_clock == TS_TIME_MAX) {
		do {
			wl_chain->wl_start_clock = tp->tp_time;
			wl_chain->wl_time = 0;

			wl_chain = wl_chain->wl_chain_next;
		} while(wl_chain != NULL);
	}
	else {
		do {
			wl_chain->wl_time += tp->tp_quantum;
			wl_chain = wl_chain->wl_chain_next;
		} while(wl_chain != NULL);
	}

	step = wl_advance_step(wl);

	if(step == NULL) {
		return;
	}

	wl_notify(wl, WLS_RUNNING, 0, "%d requests", step->wls_rq_count);

	if(wl->wl_type->wlt_wl_step) {
		wl->wl_type->wlt_wl_step(step);
	}

	if(wl->wl_type->wlt_run_request) {
		tp_distribute_requests(step, tp);
	}

	logmsg(LOG_TRACE, "Workload %s step #%ld", wl->wl_name, wl->wl_current_step);
}

/**
 * Worker thread
 */
thread_result_t worker_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, tp_worker_t, worker);

	request_t* rq;
	request_t* rq_root;

	thread_pool_t* tp = worker->w_tp;

	long worker_step = 0;

	tp_hold(tp);

	logmsg(LOG_DEBUG, "Started worker thread #%d (tpool: %s)",
			thread->t_local_id, tp->tp_name);

	while(!worker->w_tp->tp_is_dead) {
		rq_root = tp->tp_disp->tpd_class->worker_pick(tp, worker);
		if(rq_root == NULL)
			continue;

		rq = rq_root;
		do {
			wl_run_request(rq);

			rq = rq->rq_chain_next;
		} while(rq != NULL);

		tp->tp_disp->tpd_class->worker_done(tp, worker, rq_root);
	}

THREAD_END:
	tp_rele(tp, B_FALSE);
	THREAD_FINISH(arg);
}


