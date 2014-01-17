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
#include <schedutil.h>

#include <assert.h>

void tp_detach_nolock(thread_pool_t* tp, struct workload* wl);

static void control_report_step(thread_pool_t* tp);
static void control_prepare_step(thread_pool_t* tp, workload_step_t* step);

static request_t* worker_pick_request(thread_pool_t* tp, tp_worker_t* worker);
static request_t* worker_master_dispatch(thread_pool_t* tp, tp_worker_t* worker);

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

	workload_step_t* step = mp_malloc(1 * sizeof(workload_step_t));

	int wi;
	tp_worker_t* worker;

	tp_hold(tp);

	/* Synchronize all control-threads on all nodes to start
	 * quantum at beginning of next second (real time synchronization
	 * is provided by NTP) */
	tm_sleep_nano(tm_ceil_diff(tm, T_SEC));

	logmsg(LOG_DEBUG, "Started control thread (tpool: %s)", tp->tp_name);

	while(!tp->tp_is_dead) {
		tp->tp_time = tm_get_clock();

		logmsg(LOG_TRACE, "Threadpool '%s': control thread is running (tm: %lld)",
					tp->tp_name, tp->tp_time);

		mutex_lock(&tp->tp_mutex);
		wl_count = tp->tp_wl_count;

		/* = INITIALIZE STEP ARRAY = */
		if(unlikely(tp->tp_wl_changed)) {
			/* Workload count changes very rare,
			 * so it's easier to reallocate step array */
			if(unlikely(wl_count != wl_count_prev)) {
				mp_free(step);
				step = mp_malloc(wl_count * sizeof(workload_step_t));
			}

			/*Re-initialize step's workloads*/
			wli = 0;
			list_for_each_entry(workload_t, wl, &tp->tp_wl_head, wl_tp_node) {
				assert(wli < wl_count);

				step[wli].wls_workload = wl;
				++wli;
			}

			tp->tp_wl_changed = B_FALSE;
		}

		wl_count_prev = wl_count;

		control_report_step(tp);

		/*No workloads attached to thread pool, go to bed!*/
		if(wl_count == 0)
			goto quantum_sleep;

		/* Advance step for each workload, then
		 * distribute requests across workers*/
		for(wli = 0; wli < wl_count; ++wli) {
			control_prepare_step(tp, &step[wli]);
		}

		if(!list_empty(&tp->tp_rq_head)) {
			tp->tp_current_rq = list_first_entry(request_t, &tp->tp_rq_head, rq_node);
		}

		logmsg(LOG_TRACE, "Threadpool '%s': waking up workers", tp->tp_name);

		cv_notify_all(&tp->tp_cv_worker);

	quantum_sleep:
		mutex_unlock(&tp->tp_mutex);

		tm_sleep_nano(tm_ceil_diff(tm_get_time(), tp->tp_quantum));
	}

THREAD_END:
	mp_free(step);
	tp_rele(tp, B_FALSE);
	THREAD_FINISH(arg);
}

static void control_prepare_step(thread_pool_t* tp, workload_step_t* step) {
	int ret = 0;
	workload_t *wl;

	wl = step->wls_workload;

	if(unlikely(!wl_is_started(wl))) {
		return;
	}

	ret = wl_advance_step(step);

	if(ret == -1) {
		wl_finish(wl);

		/*If steps was not provided, detach workload from threadpool*/
		tp_detach_nolock(wl->wl_tp, wl);
		return;
	}

	wl_notify(wl, WLS_RUNNING, 0, "Running workload %s step #%ld: %d requests",
					wl->wl_name, wl->wl_current_step, step->wls_rq_count);

	if(wl->wl_type->wlt_wl_step) {
		wl->wl_type->wlt_wl_step(wl, step->wls_rq_count);
	}

	if(wl->wl_type->wlt_run_request) {
		/*FIXME: case if worker didn't started processing of requests for previous step yet */
		tp_distribute_requests(step, tp);
	}

	logmsg(LOG_TRACE, "Workload %s step #%ld", wl->wl_name, wl->wl_current_step);
}

static void control_report_step(thread_pool_t* tp) {
	list_head_t* rq_list = NULL;

	if(!list_empty(&tp->tp_rq_head)) {
		rq_list = (list_head_t*) mp_malloc(sizeof(list_head_t));

		list_head_init(rq_list, "rqs-%s-out", tp->tp_name);
		list_splice_init(&tp->tp_rq_head, list_head_node(rq_list));

		tp->tp_current_rq = NULL;

		wl_report_requests(rq_list);
	}
}

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
		rq_root = worker_pick_request(tp, worker);
		if(rq_root == NULL)
			continue;

		rq = rq_root;
		do {
			wl_run_request(rq, worker->w_thread.t_local_id);

			rq = rq->rq_chain_next;
		} while(rq != NULL);
	}

THREAD_END:
	tp_rele(tp, B_FALSE);
	THREAD_FINISH(arg);
}

/* TSLoad doesn't simulate request arrivals, because it imposes extra
 * context switches. Instead of doing that, first worker to enter tp_mutex
 * becomes master and dispatches next request to one of slaves (or itself).
 * */
static request_t* worker_pick_request(thread_pool_t* tp, tp_worker_t* worker) {
	request_t* rq = NULL;

	boolean_t is_master = tp->tp_master == worker;

	if(is_master) {
		/* Was chosen by prevous master - wait until it release mutex */
		mutex_lock(&tp->tp_mutex);
	}
	else {
		/* Try to became master */
		is_master = mutex_try_lock(&tp->tp_mutex);
	}

	if(is_master) {
		tp->tp_master = worker;

		if(tp->tp_current_rq != NULL) {
			rq = worker_master_dispatch(tp, worker);
		}
		else {
			logmsg(LOG_TRACE, "Threadpool '%s': worker %d is waiting for requests",
						tp->tp_name, worker->w_thread.t_local_id);

			tp->tp_master = NULL;
			cv_wait(&tp->tp_cv_worker, &tp->tp_mutex);
		}

		mutex_unlock(&tp->tp_mutex);
	}
	else {
		/* Slave */
		logmsg(LOG_TRACE, "Threadpool '%s': worker %d is slave", tp->tp_name,
						worker->w_thread.t_local_id);

		mutex_lock(&worker->w_rq_mutex);
		worker->w_rq_mailbox = NULL;
		cv_wait(&worker->w_rq_cv, &worker->w_rq_mutex);
		rq = worker->w_rq_mailbox;
		mutex_unlock(&worker->w_rq_mutex);
	}

	return rq;
}

static request_t* worker_master_dispatch(thread_pool_t* tp, tp_worker_t* self) {
	request_t* master_rq = tp->tp_current_rq;
	int wid = tp->tp_last_worker + 1, i;

	tp_worker_t* worker;

	ts_time_t cur_time = tm_get_clock();
	ts_time_t next_time;
	ts_time_t delta;

	boolean_t dispatched = B_FALSE;

	logmsg(LOG_TRACE, "Threadpool '%s': worker %d is elected as master",
						tp->tp_name, self->w_thread.t_local_id);

	if(list_is_last(&master_rq->rq_node, &tp->tp_rq_head))
		tp->tp_current_rq = NULL;
	else
		tp->tp_current_rq = list_next_entry(request_t, master_rq, rq_node);

	self->w_rq_mailbox = NULL;

	/* Sleep till request time come */
	next_time = master_rq->rq_sched_time;
	delta = tm_diff(cur_time, next_time);

	if(cur_time < next_time && delta > TP_WORKER_MIN_SLEEP) {
		tm_sleep_nano(delta - TP_WORKER_OVERHEAD);
	}

	/* Dispatch master rq to first worker
	 * TODO: request balancing */
	for(i = 0; i < tp->tp_num_threads; ++i) {
		if(wid >= tp->tp_num_threads)
			wid = 0;
		worker = tp->tp_workers + wid++;

		if(worker->w_rq_mailbox != NULL)
			continue;

		if(worker == self) {
			/* Dispatch request to master itself.
			 * If there are at least one sleeping worker, wakeup
			 * it so it can be re-elected as master */
			self->w_rq_mailbox = master_rq;
			tp->tp_last_worker = wid;
			dispatched = B_TRUE;
		}
		else {
			mutex_lock(&worker->w_rq_mutex);

			if(!dispatched) {
				worker->w_rq_mailbox = master_rq;
				tp->tp_last_worker = wid;
				dispatched = B_TRUE;
			}
			else {
				tp->tp_master = worker;
			}

			cv_notify_one(&worker->w_rq_cv);
			mutex_unlock(&worker->w_rq_mutex);

			break;
		}
	}

	if(dispatched) {
		logmsg(LOG_TRACE, "Threadpool '%s': dispatched request %s/%d to worker %d",
					tp->tp_name, master_rq->rq_workload->wl_name,
					master_rq->rq_id, tp->tp_last_worker);
	}

	return self->w_rq_mailbox;
}
