/*
 * worker.c
 *
 *  Created on: 04.12.2012
 *      Author: myaut
 */

#define LOG_SOURCE "worker"
#include <log.h>

#include <util.h>
#include <mempool.h>
#include <workload.h>
#include <defs.h>
#include <tstime.h>
#include <threads.h>
#include <threadpool.h>

#include <assert.h>

void tp_detach_nolock(thread_pool_t* tp, struct workload* wl);

/* Control thread
 *
 * Control thread notifies workers after on each quantum ending
 * and processes each step for each workload on thread pool
 * */
void* control_thread(void* arg) {
	THREAD_ENTRY(arg, thread_pool_t, tp);
	ts_time_t tm = tm_get_time();
	workload_t *wl;
	int wl_count = 0, wl_count_prev = 0, wli = 0;
	int ret = 0;
	int tid = 0;

	workload_step_t* step = mp_malloc(1 * sizeof(workload_step_t));;

	/* Synchronize all control-threads on all nodes to start
	 * quantum at beginning of next second (real time synchronization
	 * is provided by NTP) */
	tm_sleep(tm_ceil_diff(tm, SEC));

	logmsg(LOG_DEBUG, "Started control thread (tpool: %s)", tp->tp_name);

	while(!tp->tp_is_dead) {
		tp->tp_time = tm_get_time();

		logmsg(LOG_TRACE, "Control thread %s is running (tm: %llu)",
					tp->tp_name, tp->tp_time);

		mutex_lock(&tp->tp_mutex);

		wl_count = tp->tp_wl_count;

		/* = INITIALIZE STEP ARRAY = */
		if(unlikely(tp->tp_wl_changed)) {
			/* Workload count changes whery rare,
			 * so it's easier to reallocate step array */
			if(unlikely(wl_count != wl_count_prev)) {
				mp_free(step);
				step = mp_malloc(wl_count * sizeof(workload_step_t));
			}

			/*Re-initialize step's workloads*/
			wli = 0;
			list_for_each_entry(wl, &tp->tp_wl_head, wl_tp_node) {
				step[wli].wls_workload = wl;
				++wli;

				assert(wli < wl_count);
			}

			tp->tp_wl_changed = FALSE;
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
				/*If steps was not provided, detach workload from threadpool*/
				tp_detach_nolock(wl->wl_tp, wl);

				/*FIXME: unconfig detached works*/
				continue;
			}

			/*FIXME: case if worker didn't started processing of requests for previous step yet */
			tp_distribute_requests(step + wli, tp);

			logmsg(LOG_TRACE, "Workload %s step #%ld",
					wl->wl_name, wl->wl_current_step);
		}

		/* FIXME: should shuffle requests */

		/* Unlock mutexes and notify worker thread that we are ready*/
		for(tid = 0; tid < tp->tp_num_threads; ++tid)
			mutex_unlock(&tp->tp_workers[tid].w_rq_mutex);


		/* = START WORKERS = */
		event_notify_all(&tp->tp_event);

	quantum_sleep:
		mutex_unlock(&tp->tp_mutex);

		tm_sleep(tm_ceil_diff(tm_get_time(), tp->tp_quantum));
	}

THREAD_END:
	THREAD_FINISH(arg);
}

void* worker_thread(void* arg) {
	THREAD_ENTRY(arg, tp_worker_t, worker);
	list_head_t* rq_chain;
	request_t* rq;

	thread_pool_t* tp = worker->w_tp;

	logmsg(LOG_DEBUG, "Started worker thread #%d (tpool: %s)",
			thread->t_local_id, tp->tp_name);

	while(!worker->w_tp->tp_is_dead) {
		rq_chain = (list_head_t*) mp_malloc(sizeof(list_head_t));
		list_head_init(rq_chain, "rqs-%s-%d", tp->tp_name, thread->t_local_id);

		/* Extract requests from queue */
		mutex_lock(&worker->w_rq_mutex);
		list_splice_init(&worker->w_requests, list_head_node(rq_chain));
		mutex_unlock(&worker->w_rq_mutex);

		/*FIXME: quantum exhaustion*/
		list_for_each_entry(rq, rq_chain, rq_node)  {
			wl_run_request(rq);
		}

		/*Push chain of requests to tp's chain*/
		squeue_push(&tp->tp_requests, rq_chain);

		event_wait(&tp->tp_event);
	}

THREAD_END:
	THREAD_FINISH(arg);
}
