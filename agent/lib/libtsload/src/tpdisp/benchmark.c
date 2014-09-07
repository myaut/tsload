
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



#include <tpdisp.h>
#include <threadpool.h>
#include <threads.h>
#include <mempool.h>
#include <tstime.h>
#include <workload.h>
#include <list.h>

/**
 * #### Benchmark dispatcher
 *
 * Based on first-free dispatcher, but much simpler. Dispatching is handled by worker threads.
 *
 * tpd_bench_t contains cyclic list of requests bench_rq_list (copy of tp_rq_head list), and
 * each worker picks next entry from that list, clones it and runs request. On step end,
 * tpd_worker_pick_bench swaps lists.
 */

typedef struct {
	thread_mutex_t bench_mutex;
	request_t* bench_current_rq;
	list_head_t* bench_rq_list;
	list_head_t bench_finished;
} tpd_bench_t;

int tpd_init_bench(thread_pool_t* tp) {
	tpd_bench_t* bench = mp_malloc(sizeof(tpd_bench_t));

	mutex_init(&bench->bench_mutex, "bench-mutex");
	list_head_init(&bench->bench_finished, "bench-finished");

	bench->bench_rq_list = NULL;
	bench->bench_current_rq = NULL;

	tp->tp_disp->tpd_data = bench;

	return TPD_OK;
}

void tpd_destroy_bench(thread_pool_t* tp) {
	tpd_bench_t* bench = (tpd_bench_t*) tp->tp_disp->tpd_data;

	mutex_destroy(&bench->bench_mutex);

	mp_free(bench);
}

void tpd_control_sleep_bench(thread_pool_t* tp) {
	tpd_bench_t* bench = (tpd_bench_t*) tp->tp_disp->tpd_data;
	ts_time_t cur_time;

	int wid;

	list_head_t* rq_list = NULL;

	/* Swap request lists then wake up all workers. */

	if(!list_empty(&tp->tp_rq_head)) {
		rq_list = (list_head_t*) mp_malloc(sizeof(list_head_t));
		list_head_init(rq_list, "rqs-%s-in", tp->tp_name);
		list_splice_init(&tp->tp_rq_head, list_head_node(rq_list));
	}

	mutex_lock(&bench->bench_mutex);
	bench->bench_rq_list = rq_list;
	if(rq_list != NULL) {
		bench->bench_current_rq = list_first_entry(request_t, rq_list, rq_node);
	}
	mutex_unlock(&bench->bench_mutex);

	for(wid = 0; wid < tp->tp_num_threads; ++wid) {
		tpd_wqueue_signal(tp, wid);
	}

	cur_time = tm_get_clock();
	if(cur_time < (tp->tp_time + tp->tp_quantum))
		tm_sleep_nano(tm_diff(cur_time, tp->tp_time + tp->tp_quantum));
}

void tpd_control_report_bench(thread_pool_t* tp) {
	tpd_bench_t* bench = (tpd_bench_t*) tp->tp_disp->tpd_data;
	list_head_t* rq_list = (list_head_t*) mp_malloc(sizeof(list_head_t));

	list_head_t* old_rq_list = NULL;

	list_head_init(rq_list, "rqs-%s-out", tp->tp_name);

	/* Detach request list. We could do this in tpd_control_sleep_bench() and
	 * reduce interstep-effects by keeping bench->bench_rq_list and workers would be busy,
	 * but if workload would be detached (it detaches between report/sleep stages),
	 * tsload will segfault. */

	mutex_lock(&bench->bench_mutex);

	old_rq_list = bench->bench_rq_list;
	bench->bench_rq_list = NULL;
	list_splice_init(&bench->bench_finished, list_head_node(rq_list));

	mutex_unlock(&bench->bench_mutex);

	if(old_rq_list != NULL) {
		/* FIXME: do not allocate rq_list each time */
		wl_destroy_request_list(old_rq_list);
		mp_free(old_rq_list);
	}

	wl_report_requests(rq_list);
}

request_t* tpd_worker_pick_bench(thread_pool_t* tp, tp_worker_t* worker) {
	tpd_bench_t* bench = (tpd_bench_t*) tp->tp_disp->tpd_data;
	request_t* rq = NULL;
	request_t* rq_current = NULL;

	mutex_lock(&bench->bench_mutex);

	/* There are no requests - wait until control thread will update bench_rq_list */
	while(bench->bench_rq_list == NULL) {
		mutex_unlock(&bench->bench_mutex);
		tpd_worker_wait(tp, worker->w_thread.t_local_id);

		if(tp->tp_is_dead) {
			return NULL;
		}

		mutex_lock(&bench->bench_mutex);
	}

	rq_current = bench->bench_current_rq;
	rq = wl_clone_request(rq_current);

	/* Pick next request */
	if(list_is_last(&rq_current->rq_node, bench->bench_rq_list)) {
		bench->bench_current_rq = list_first_entry(request_t, bench->bench_rq_list, rq_node);
	}
	else {
		bench->bench_current_rq = list_next_entry(request_t, rq_current, rq_node);
	}

	mutex_unlock(&bench->bench_mutex);

	rq->rq_thread_id = worker->w_thread.t_local_id;

	return rq;
}

void tpd_worker_done_bench(thread_pool_t* tp, tp_worker_t* worker, request_t* rq) {
	tpd_bench_t* bench = (tpd_bench_t*) tp->tp_disp->tpd_data;

	mutex_lock(&bench->bench_mutex);
	list_add_tail(&rq->rq_node, &bench->bench_finished);
	mutex_unlock(&bench->bench_mutex);
}

void tpd_relink_request_bench(thread_pool_t* tp, request_t* rq) {
	/* Relinking not supported for benchmarking
	 * (thus, its useless cause no scheduling is made) */
}

tp_disp_class_t tpd_bench_class = {
	"benchmark",

	tpd_init_bench,
	tpd_destroy_bench,
	tpd_control_report_bench,
	tpd_control_sleep_bench,
	tpd_worker_pick_bench,
	tpd_worker_done_bench,
	tpd_wqueue_signal,
	tpd_relink_request_bench
};

