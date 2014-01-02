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

#include <assert.h>

void tp_detach_nolock(thread_pool_t* tp, struct workload* wl);

/* Control thread
 *
 * Control thread notifies workers after each quantum ending
 * and processes each step for each workload on thread pool
 * */
thread_result_t control_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, thread_pool_t, tp);
	ts_time_t tm = tm_get_time();
	workload_t *wl;
	int wl_count = 0, wl_count_prev = 0, wli = 0;
	int ret = 0;
	int tid = 0;

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

		logmsg(LOG_TRACE, "Control thread %s is running (tm: %lld)",
					tp->tp_name, tp->tp_time);

		mutex_lock(&tp->tp_mutex);

		wl_count = tp->tp_wl_count;

		/* = RESET WORKERS = */
		for(wi = 0; wi < tp->tp_num_threads; ++wi) {
			atomic_set(&tp->tp_workers[wi].w_state, INTERRUPT);
		}

		while(tp->tp_ready_workers < tp->tp_num_threads)
			cv_wait(&tp->tp_cv_control, &tp->tp_mutex);

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

		/*No workloads attached to thread pool, go to bed!*/
		if(wl_count == 0)
			goto quantum_sleep;


		/* = ADVANCE STEP = */
		for(tid = 0; tid < tp->tp_num_threads; ++tid)
				mutex_lock(&tp->tp_workers[tid].w_rq_mutex);

		/* Advance step for each workload, then
		 * distribute requests across workers*/
		for(wli = 0; wli < wl_count; ++wli) {
			wl = step[wli].wls_workload;

			if(unlikely(!wl_is_started(wl))) {
				continue;
			}

			ret = wl_advance_step(step + wli);

			if(ret == -1) {
				wl_notify(wl, WLS_FINISHED, 0, "Finished workload %s on step #%ld",
								wl->wl_name, wl->wl_current_step);

				/*If steps was not provided, detach workload from threadpool*/
				tp_detach_nolock(wl->wl_tp, wl);

				continue;
			}

			wl_notify(wl, WLS_RUNNING, 0, "Running workload %s step #%ld: %d requests",
							wl->wl_name, wl->wl_current_step, step[wli].wls_rq_count);

			if(wl->wl_type->wlt_wl_step) {
				wl->wl_type->wlt_wl_step(wl, step[wli].wls_rq_count);
			}

			if(wl->wl_type->wlt_run_request) {
				/*FIXME: case if worker didn't started processing of requests for previous step yet */
				tp_distribute_requests(step + wli, tp);
			}

			logmsg(LOG_TRACE, "Workload %s step #%ld",
					wl->wl_name, wl->wl_current_step);
		}

		/* FIXME: should shuffle requests */

		/* Unlock mutexes and notify worker thread that we are ready*/
		for(tid = 0; tid < tp->tp_num_threads; ++tid)
			mutex_unlock(&tp->tp_workers[tid].w_rq_mutex);

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

thread_result_t worker_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, tp_worker_t, worker);
	list_head_t* rq_chain;
	request_t* rq;

	thread_pool_t* tp = worker->w_tp;

	ts_time_t cur_time;
	ts_time_t next_time;
	ts_time_t delta;

	long worker_step = 0;

	tp_hold(tp);

	logmsg(LOG_DEBUG, "Started worker thread #%d (tpool: %s)",
			thread->t_local_id, tp->tp_name);

	while(!worker->w_tp->tp_is_dead) {
		mutex_lock(&worker->w_rq_mutex);

		atomic_set(&worker->w_state, WORKING);

		mutex_lock(&tp->tp_mutex);
		--tp->tp_ready_workers;
		mutex_unlock(&tp->tp_mutex);

		if(!list_empty(&worker->w_requests)) {
			/*There are new requests on queue*/
			cur_time = tm_get_clock();

			rq_chain = (list_head_t*) mp_malloc(sizeof(list_head_t));
			list_head_init(rq_chain, "rqs-%s-%d", tp->tp_name, thread->t_local_id);

			/* Extract requests from queue */
			list_splice_init(&worker->w_requests, list_head_node(rq_chain));
			mutex_unlock(&worker->w_rq_mutex);

			list_for_each_entry(request_t, rq, rq_chain, rq_node)  {
				next_time = rq->rq_sched_time;
				delta = tm_diff(cur_time, next_time);

				if(cur_time < next_time && delta > TP_WORKER_MIN_SLEEP) {
					tm_sleep_nano(delta - TP_WORKER_OVERHEAD);
				}

				wl_run_request(rq);

				/* Do not bother OS, if we recently asked about time */
				cur_time = (rq->rq_end_time != 0)
						? rq->rq_end_time
						: tm_get_clock();

				if(atomic_read(&worker->w_state) == INTERRUPT) {
					break;
				}
			}

			/*Push chain of requests to global chain*/
			wl_rq_chain_push(rq_chain);
		}
		else {
			/* No requests, sweet dreams, worker */
			mutex_unlock(&worker->w_rq_mutex);
		}

		mutex_lock(&tp->tp_mutex);
		if(!worker->w_tp->tp_is_dead) {
			++tp->tp_ready_workers;
			cv_notify_one(&tp->tp_cv_control);
			atomic_set(&worker->w_state, SLEEPING);
			cv_wait(&tp->tp_cv_worker, &tp->tp_mutex);
		}
		mutex_unlock(&tp->tp_mutex);
	}

THREAD_END:
	assert(list_empty(&worker->w_requests));
	tp_rele(tp, B_FALSE);
	THREAD_FINISH(arg);
}
