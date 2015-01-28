
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



#define LOG_SOURCE "tpool"
#include <tsload/log.h>

#include <tsload/defs.h>

#include <tsload/time.h>
#include <tsload/mempool.h>
#include <tsload/errcode.h>
#include <tsload/hashmap.h>

#include <tsload/load/tpdisp.h>
#include <tsload/load/workload.h>
#include <tsload.h>

#include <errormsg.h>
#include <tsloadimpl.h>

#include <stdlib.h>
#include <string.h>

DECLARE_HASH_MAP_STRKEY(tpd_hash_map, tp_disp_class_t, TPDHASHSIZE, name, next, TPDHASHMASK);

extern tp_disp_class_t tpd_rr_class;
extern tp_disp_class_t tpd_rand_class;
extern tp_disp_class_t tpd_user_class;
extern tp_disp_class_t tpd_trace_class;
extern tp_disp_class_t tpd_fill_up_class;
extern tp_disp_class_t tpd_ff_class;
extern tp_disp_class_t tpd_bench_class;

extern ts_time_t tp_worker_min_sleep;

extern ts_time_t tp_worker_overhead;

int tpd_preinit_fill_up(tp_disp_t* tpd, unsigned num_requests, int first_wid);

boolean_t tpd_wait_for_arrival(request_t* rq, ts_time_t sleep_not_until) {
	ts_time_t cur_time, next_time;
	ts_time_t sleep_time;
	ts_time_t max_sleep;

	cur_time = tm_get_clock();
	next_time = rq->rq_sched_time + rq->rq_workload->wl_start_clock;
	sleep_time = tm_diff(cur_time, next_time) - tp_worker_overhead;
	max_sleep = tm_diff(cur_time, sleep_not_until);

	if(sleep_time > max_sleep) {
		if(sleep_not_until > cur_time)
			tm_sleep_nano(max_sleep);
		return B_FALSE;
	}

	if(cur_time < next_time && sleep_time > tp_worker_min_sleep) {
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
	rq->rq_flags |= RQF_DEQUEUED;

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

void tpd_worker_wait(thread_pool_t* tp, int wid) {
	tp_worker_t* worker = tp->tp_workers + wid;

	mutex_lock(&worker->w_rq_mutex);
	cv_wait(&worker->w_rq_cv, &worker->w_rq_mutex);
	mutex_unlock(&worker->w_rq_mutex);
}

void tpd_wqueue_signal(thread_pool_t* tp, int wid) {
	tp_worker_t* worker = tp->tp_workers + wid;

	mutex_lock(&worker->w_rq_mutex);
	cv_notify_one(&worker->w_rq_cv);
	mutex_unlock(&worker->w_rq_mutex);
}

tsobj_node_t* tsobj_tpd_class_format(tp_disp_class_t* tpd_class) {
	tsobj_node_t* node = tsobj_new_node("tsload.tp.ThreadpoolDispatcher");
	
	tsobj_add_string(node, TSOBJ_STR("description"), 
					 tsobj_str_create(tpd_class->description));
	tsobj_add_node(node, TSOBJ_STR("params"), 
				   tsobj_params_format_helper(tpd_class->params));
	
	tsobj_module_format_helper(node, tpd_class->mod);
	
	return node;
}

tp_disp_t* tsobj_tp_disp_proc(tsobj_node_t* node) {
	tp_disp_t* tpd = NULL;
	tp_disp_class_t* tpd_class;
	char* type;
	int err;
	
	if(tsobj_get_string(node, "type", &type) != TSOBJ_OK)
		goto bad_tsobj;
	
	tpd_class = hash_map_find(&tpd_hash_map, type);
	if(tpd_class == NULL) {
		tsload_error_msg(TSE_INVALID_DATA,
						 TPD_ERROR_PREFIX "invalid type '%s'", type);
		return NULL;
	}

	tpd = mp_malloc(sizeof(tp_disp_t));
	tpd->tpd_tp = NULL;
	tpd->tpd_data = NULL;
	tpd->tpd_class = tpd_class;
	
	if(tpd_class->proc_tsobj) {
		err = tpd_class->proc_tsobj(tpd, node);	
		if(err != TPD_OK) {
			mp_free(tpd);
			if(err == TPD_BAD)
				goto bad_tsobj;
			return NULL;
		}
	}

	if(tsobj_check_unused(node) != TSOBJ_OK) {
		mp_free(tpd);
		goto bad_tsobj;
	}

	return tpd;

bad_tsobj:
	tsload_error_msg(tsobj_error_code(), TPD_ERROR_PREFIX "%s", tsobj_error_message());
	return NULL;
}

void tpd_destroy(tp_disp_t* tpd) {
	tpd->tpd_class->destroy(tpd->tpd_tp);
	mp_free(tpd);
}

int tpdisp_register(module_t* mod, tp_disp_class_t* class) {
	class->next = NULL;
	class->mod = mod;
	
	return hash_map_insert(&tpd_hash_map, class);
}

int tpdisp_unregister(module_t* mod, tp_disp_class_t* class) {
	return hash_map_remove(&tpd_hash_map, class);
}

int tpdisp_init(void) {
	hash_map_init(&tpd_hash_map, "tpd_hash_map");
	
	tpdisp_register(NULL, &tpd_rr_class);
	tpdisp_register(NULL, &tpd_rand_class);
	tpdisp_register(NULL, &tpd_user_class);
	tpdisp_register(NULL, &tpd_trace_class);
	tpdisp_register(NULL, &tpd_fill_up_class);
	tpdisp_register(NULL, &tpd_ff_class);
	tpdisp_register(NULL, &tpd_bench_class);
	
	return 0;
}

void tpdisp_fini(void) {
	tpdisp_unregister(NULL, &tpd_bench_class);
	tpdisp_unregister(NULL, &tpd_ff_class);
	tpdisp_unregister(NULL, &tpd_fill_up_class);
	tpdisp_unregister(NULL, &tpd_trace_class);
	tpdisp_unregister(NULL, &tpd_user_class);
	tpdisp_unregister(NULL, &tpd_rand_class);
	tpdisp_unregister(NULL, &tpd_rr_class);
	
	hash_map_destroy(&tpd_hash_map);
}