
/*
    This file is part of TSLoad.
    Copyright 2012-2014, Sergey Klyaus, ITMO University

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

#include <tsload/hashmap.h>
#include <tsload/mempool.h>
#include <tsload/list.h>
#include <tsload/tuneit.h>
#include <tsload/autostring.h>
#include <tsload/schedutil.h>

#include <hostinfo/cpuinfo.h>

#include <tsload/load/threadpool.h>
#include <tsload/load/workload.h>
#include <tsload.h>
#include <tsload/load/tpdisp.h>

#include <errormsg.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


mp_cache_t	   tp_cache;
mp_cache_t	   tp_worker_cache;

DECLARE_HASH_MAP_STRKEY(tp_hash_map, thread_pool_t, TPHASHSIZE, tp_name, tp_next, TPHASHMASK);

/* Minimum quantum duration. Should be set to system's tick
 * Maximum reasonable quantum duration. Currently 10 min*/
ts_time_t tp_min_quantum = TP_MIN_QUANTUM;
ts_time_t tp_max_quantum = TP_MAX_QUANTUM;

boolean_t tp_collector_enabled = B_TRUE;
ts_time_t tp_collector_interval = T_SEC / 2;

int tp_max_threads = TPMAXTHREADS;

/**
 * tunable: minimum time worker (or control thread) will sleep waiting for request arrival.
 * Because OS have granular scheduling if we go to sleep, for example for 100us, we may wakeup
 * only after 178us because nanosleep is not precise (however OS may use high-resolution timer).
 *
 * So TSLoad may prefer early arrival and ignore sleeping if time interval lesser than tp_worker_min_sleep
 * */
ts_time_t tp_worker_min_sleep = TP_WORKER_MIN_SLEEP;

/**
 * tunable: estimated time consumed by worker between handling of arrival and calling module's
 * mod_run_request method. Like tp_worker_min_sleep used to make arrivals more precise
 */
ts_time_t tp_worker_overhead = TP_WORKER_OVERHEAD;

static thread_t  t_tp_collector;

static thread_mutex_t tp_collect_mutex;
static thread_cv_t tp_collect_cv;
static int tp_count = 0;

void* worker_thread(void* arg);
void* control_thread(void* arg);

static void tp_start_threads(thread_pool_t* tp);
static void tp_destroy_impl(thread_pool_t* tp, boolean_t may_remove);

static char* worker_affinity_print(thread_pool_t* tp, int tid) {
	tp_worker_t* worker = tp->tp_workers + tid;
	size_t len = 0, capacity = 256;
	char* str = NULL;

	boolean_t first = B_TRUE;

	cpumask_t* mask = cpumask_create();

	hi_object_t *object;
	hi_cpu_object_t *cpu_object;

	list_head_t* list = hi_cpu_list(B_FALSE);

	/* Get affinity of worker */
	if(sched_get_affinity(&worker->w_thread, mask) != SCHED_OK) {
		goto end;
	}

	str = mp_malloc(capacity);

	/* Walk CPU strands. If they set in mask - append to string */
	hi_for_each_object(object, list) {
		cpu_object = HI_CPU_FROM_OBJ(object);

		if(cpu_object->type == HI_CPU_STRAND) {
			if(cpumask_isset(mask, cpu_object->id)) {
				if(first) {
					len = snprintf(str, capacity, "%d", cpu_object->id);
					first = B_FALSE;

					continue;
				}

				if((capacity - len) < 8) {
					capacity += 256;
					str = mp_realloc(str, capacity);
				}

				len += snprintf(str + len, capacity - len, ",%d", cpu_object->id);
			}
		}
	}

end:
	cpumask_destroy(mask);
	return str;
}

static char* worker_sched_print(thread_pool_t* tp, int wid) {
	tp_worker_t* worker = tp->tp_workers + wid;
	size_t len = 0, capacity = 256;
	char* str = mp_malloc(capacity);
	sched_policy_t* policy;
	sched_param_t* param;
	int64_t value;

	if(sched_get_policy(&worker->w_thread, str, capacity) != SCHED_OK) {
		mp_free(str);
		return NULL;
	}

	len = strlen(str);
	policy = sched_policy_find_byname(str);

	if(policy == NULL)
		return str;

	param = &policy->params[0];
	while(param->name != NULL) {
		if(sched_get_param(&worker->w_thread, param->name, &value) == SCHED_OK) {
			if((capacity - len) < (24 + strlen(param->name))) {
				capacity += 256;
				str = mp_realloc(str, capacity);
			}

			len += snprintf(str + len, capacity - len, " %s:%lld",
								param->name, (long long) value);
		}

		++param;
	}

	return str;
}

static void tp_create_worker(thread_pool_t* tp, int tid) {
	tp_worker_t* worker = tp->tp_workers + tid;

	mutex_init(&worker->w_rq_mutex, "worker-%s-%d", tp->tp_name, tid);
	cv_init(&worker->w_rq_cv, "worker-%s-%d", tp->tp_name, tid);
	list_head_init(&worker->w_rq_head, "worker-%s-%d", tp->tp_name, tid);

	worker->w_tp = tp;
	worker->w_tpd_data = NULL;
}

static void tp_destroy_worker(thread_pool_t* tp, int tid) {
	tp_worker_t* worker = tp->tp_workers + tid;

	if(tp->tp_started)
		t_destroy(&worker->w_thread);

    cv_destroy(&worker->w_rq_cv);
    mutex_destroy(&worker->w_rq_mutex);
}


/**
 * Create new thread pool
 *
 * @param num_threads Numbber of worker threads in this pool
 * @param name Name of thread pool
 * @param quantum Worker's quantum
 * */
thread_pool_t* tp_create(const char* name, unsigned num_threads, ts_time_t quantum,
					     boolean_t discard, struct tp_disp* disp) {
	thread_pool_t* tp = NULL;
	int tid;
	int ret;

	if(num_threads > tp_max_threads || num_threads == 0) {
		logmsg(LOG_WARN, "Failed to create thread_pool %s: maximum %d threads allowed (%d requested)",
				name, tp_max_threads, num_threads);
		return NULL;
	}

	if(quantum < tp_min_quantum) {
		logmsg(LOG_WARN, "Failed to create thread_pool %s: too small quantum %lld (%lld minimum)",
						name, quantum, tp_min_quantum);
		return NULL;
	}

	tp = (thread_pool_t*) mp_cache_alloc(&tp_cache);

	aas_copy(aas_init(&tp->tp_name), name);

	tp->tp_num_threads = num_threads;
	tp->tp_workers = (tp_worker_t*) mp_cache_alloc_array(&tp_worker_cache, num_threads);

	tp->tp_time	   = 0ll;	   /*Time is set by control thread*/
	tp->tp_quantum = quantum;

	tp->tp_is_dead = B_FALSE;
	tp->tp_wl_changed = B_FALSE;
	tp->tp_started = B_FALSE;

	tp->tp_wl_count = 0;

	tp->tp_next = NULL;

	tp->tp_ref_count = (atomic_t) 0l;
	tp_hold(tp);

	/*Initialize objects*/
	list_head_init(&tp->tp_wl_head, "tp-%s", name);
	list_head_init(&tp->tp_rq_head, "tp-rq-%s", name);

    mutex_init(&tp->tp_mutex, "tp-%s", name);

    tp->tp_discard = discard;

	for(tid = 0; tid < num_threads; ++tid) {
		tp_create_worker(tp, tid);
	}

	tp->tp_disp = disp;
	disp->tpd_tp = tp;
	ret = disp->tpd_class->init(tp);

	if(ret != TPD_OK) {
		tp_destroy_impl(tp, B_FALSE);
		return NULL;
	}

	mutex_lock(&tp_collect_mutex);
	++tp_count;
	mutex_unlock(&tp_collect_mutex);

	tp_start_threads(tp);

	hash_map_insert(&tp_hash_map, tp);

	logmsg(LOG_INFO, "Created thread pool %s with %d threads", name, num_threads);

	return tp;
}

static void tp_start_threads(thread_pool_t* tp) {
	tp_worker_t* worker;
	int wid;

	for(wid = 0; wid < tp->tp_num_threads; ++wid) {
		worker = tp->tp_workers + wid;

		t_init(&worker->w_thread, (void*) worker, worker_thread,
					"work-%s-%d", tp->tp_name, wid);
		worker->w_thread.t_local_id = WORKER_TID + wid;
	}

	t_init(&tp->tp_ctl_thread, (void*) tp, control_thread,
			"tp-ctl-%s", tp->tp_name);
	tp->tp_ctl_thread.t_local_id = CONTROL_TID;

	tp->tp_started = B_TRUE;
}


static void tp_destroy_impl(thread_pool_t* tp, boolean_t may_remove) {
	int tid;
	workload_t *wl, *tmp;

	if(may_remove)
		hash_map_remove(&tp_hash_map, tp);

	assert(list_empty(&tp->tp_wl_head));

	tp->tp_discard = B_TRUE;
	tp->tp_disp->tpd_class->control_report(tp);
	tpd_destroy(tp->tp_disp);

	for(tid = 0; tid < tp->tp_num_threads; ++tid) {
		tp_destroy_worker(tp, tid);
	}

	if(tp->tp_started) {
		t_destroy(&tp->tp_ctl_thread);
	}

	mutex_destroy(&tp->tp_mutex);

	aas_free(&tp->tp_name);

	mp_cache_free_array(&tp_worker_cache, tp->tp_workers, tp->tp_num_threads);
	mp_cache_free(&tp_cache, tp);

	mutex_lock(&tp_collect_mutex);
	--tp_count;
	cv_notify_one(&tp_collect_cv);
	mutex_unlock(&tp_collect_mutex);
}

void tp_destroy(thread_pool_t* tp) {
	int tid;
	tp_worker_t* worker;

	mutex_lock(&tp->tp_mutex);
	tp->tp_is_dead = B_TRUE;
	mutex_unlock(&tp->tp_mutex);

	/* Notify workers that we are done */
	for(tid = 0; tid < tp->tp_num_threads; ++tid) {
		tp->tp_disp->tpd_class->worker_signal(tp, tid);
	}

	tp_rele(tp, B_TRUE);
}

thread_pool_t* tp_search(const char* name) {
	return hash_map_find(&tp_hash_map, name);
}

void tp_hold(thread_pool_t* tp) {
	atomic_inc(&tp->tp_ref_count);
}

/**
 * @param may_destroy - may tp_rele free thread_pool
 *
 * Some tp_rele's may be called from control/worker thread
 * In this case we may deadlock because we will join ourselves
 * Do not destroy tp in this case - leave it to collector/tp_fini
 * */
void tp_rele(thread_pool_t* tp, boolean_t may_destroy) {
	if(atomic_dec(&tp->tp_ref_count) == 1l && may_destroy) {
		tp_destroy_impl(tp, B_TRUE);
	}
}

/**
 * Insert workload into thread pool's list
 */
void tp_attach(thread_pool_t* tp, struct workload* wl) {
	assert(wl->wl_tp == tp);

	mutex_lock(&tp->tp_mutex);

	list_add_tail(&wl->wl_tp_node, &tp->tp_wl_head);
	++tp->tp_wl_count;
	tp->tp_wl_changed = B_TRUE;

	mutex_unlock(&tp->tp_mutex);

	tp_hold(tp);
}

void tp_detach_nolock(thread_pool_t* tp, struct workload* wl) {
	mutex_lock(&wl->wl_status_mutex);

	if(wl->wl_status < WLS_FINISHED)
		wl->wl_status = WLS_FINISHED;

	wl->wl_tp = NULL;
	mutex_unlock(&wl->wl_status_mutex);

	list_del(&wl->wl_tp_node);
	--tp->tp_wl_count;
	tp->tp_wl_changed = B_TRUE;

	tp_rele(tp, B_FALSE);
}

/**
 * Remove workload from thread pool list
 */
void tp_detach(thread_pool_t* tp, struct workload* wl) {
	assert(wl->wl_tp == tp);

	mutex_lock(&tp->tp_mutex);
	tp_detach_nolock(tp, wl);
	mutex_unlock(&tp->tp_mutex);
}

/**
 * Compare two requests. Returns >= 0 if request rq2 is going after rq1
 */
int tp_compare_requests(struct request* rq1, struct request* rq2) {
	ts_time_t time1 = rq1->rq_sched_time + rq1->rq_workload->wl_start_clock;
	ts_time_t time2 = rq2->rq_sched_time + rq2->rq_workload->wl_start_clock;

	if(time2 > time1)
		return 100;
	if(time2 < time1)
		return -100;


	return rq2->rq_id - rq1->rq_id;
}

#define NODE_TO_REQUEST(rq_node, offset)    ((request_t*)(((char*) (rq_node)) - offset))
#define REQUEST_TO_NODE(rq, offset)    		((list_node_t*)(((char*) (rq)) + offset))

static void tp_trace_insert_request(request_t* rq, list_head_t* rq_list, ptrdiff_t offset) {
	request_t* prev_rq = NULL;
	request_t* next_rq = NULL;

	list_node_t* rq_node = REQUEST_TO_NODE(rq, offset);

	if(rq_node->next != &rq_list->l_head) {
		next_rq = NODE_TO_REQUEST(rq_node->next, offset);
	}
	if(rq_node->prev != &rq_list->l_head) {
		prev_rq = NODE_TO_REQUEST(rq_node->prev, offset);
	}

	logmsg(LOG_TRACE, "Linking request %s/%d (%lld) prev: %s/%d (%lld) next: %s/%d (%lld)",
		   rq->rq_workload->wl_name, rq->rq_id,  (long long) rq->rq_sched_time,
		   (prev_rq)? prev_rq->rq_workload->wl_name : "???", (prev_rq)? prev_rq->rq_id : -1,
		   (prev_rq)? (long long) prev_rq->rq_sched_time : -1,
		   (next_rq)? next_rq->rq_workload->wl_name : "???", (next_rq)? next_rq->rq_id : -1,
		   (next_rq)? (long long) next_rq->rq_sched_time : -1);
}

/**
 * Insert request into request queue. Keeps requests sorted by their sched-time.
 * Doesn't walks entire list, but uses prev_rq and next_rq as hint parameters.
 *
 * I.e. if we have requests on queue with schedule times 10,20,30,40,50 and 60
 * and want to add request from another workload with sched time 45, prev request
 * should point to 60, and we walk back. Then we will get prev_rq = 40 and next_rq
 * is 50, so to insert request with 55, we shouldn't walk entire list of requests.
 *
 * @param rq_list request queue
 * @param rq request to be inserted
 * @param p_prev_rq hint that contains previous request inside queue
 * @param p_next_rq hint that contains next request inside queue
 */
void tp_insert_request_impl(list_head_t* rq_list, list_node_t* rq_node,
					   list_node_t** p_prev_node, list_node_t** p_next_node, ptrdiff_t offset) {
	list_node_t* prev_rq_node = *p_prev_node;
	list_node_t* next_rq_node = *p_next_node;

	request_t* rq = NODE_TO_REQUEST(rq_node, offset);
	list_node_t* tmp_node = prev_rq_node;

	assert(rq != NULL);

	/* Usually there are only one workload per threadpool, so we may simple put
	 * new request after last request. But if it is second workload, these conditions
	 * would not be satisfied (there would be requests after current request, and
	 * we have to find appropriate place for it. */
	if(list_empty(rq_list)) {
		list_add_tail(rq_node, rq_list);
		prev_rq_node = rq_node;
		next_rq_node = rq_node;
	}
	else {
		boolean_t middle = B_FALSE;

		if(tp_compare_requests(rq, NODE_TO_REQUEST(next_rq_node, offset)) < 0) {
			/* Search forward */
			prev_rq_node = next_rq_node;

			list_for_each_continue(next_rq_node, rq_list) {
				if(tp_compare_requests(rq, NODE_TO_REQUEST(next_rq_node, offset)) >= 0) {
					middle = B_TRUE;
					break;
				}
				prev_rq_node = next_rq_node;
			}

			if(!middle) {
				list_add_tail(rq_node, rq_list);
				next_rq_node = rq_node;
			}
		}
		else if(tp_compare_requests(rq, NODE_TO_REQUEST(prev_rq_node, offset)) >= 0) {
			/* Search backward */
			next_rq_node = prev_rq_node;

			list_for_each_continue_reverse(prev_rq_node, rq_list) {
				if(tp_compare_requests(NODE_TO_REQUEST(prev_rq_node, offset), rq) >= 0) {
					middle = B_TRUE;
					break;
				}
				next_rq_node = prev_rq_node;
			}

			if(!middle) {
				list_add(rq_node, rq_list);
				prev_rq_node = rq_node;
			}
		}
		else {
			middle = B_TRUE;
		}

		if(middle) {
			/* Insert new request between next_rq and last_rq */
			__list_add(rq_node, prev_rq_node, next_rq_node);
			prev_rq_node = rq_node;
		}
	}

	tp_trace_insert_request(rq, rq_list, offset);

	if(prev_rq_node != next_rq_node) {
		assert(prev_rq_node->next == next_rq_node);
		assert(next_rq_node->prev == prev_rq_node);
	}

	*p_prev_node = prev_rq_node;
	*p_next_node = next_rq_node;
}

/**
 * Make preliminary initialization of previous and next node if
 * list already contains requests. Sets prev and next node to NULL
 * otherwise.
 */
void tp_insert_request_initnodes(list_head_t* rq_list, list_node_t** p_prev_node, list_node_t** p_next_node) {
	list_node_t* prev_rq_node = NULL;
	list_node_t* next_rq_node = NULL;

	if(!list_empty(rq_list)) {
		prev_rq_node = list_head_node(rq_list)->next;

		if(list_is_singular(rq_list)) {
			next_rq_node = prev_rq_node;
		}
		else {
			next_rq_node = prev_rq_node->next;
		}
	}

	*p_prev_node = prev_rq_node;
	*p_next_node = next_rq_node;
}

/**
 * Create requests instances according to step data or attach
 * trace-based requests to threadpool request queue. Automatically sorts
 * requests by its arrival time.
 *
 * Distribution across workers is actually done by threadpool dispatcher. */
void tp_distribute_requests(workload_step_t* step, thread_pool_t* tp) {
	unsigned rq_count = step->wls_rq_count;

	request_t* rq;
	request_t* rq_chain;
	request_t* prev_rq = NULL;
	request_t* next_rq = NULL;

	list_head_t* rq_list = &tp->tp_rq_head;
	list_node_t* prev_rq_node = NULL;
	list_node_t* next_rq_node = NULL;

	int tid;

	tp_insert_request_initnodes(rq_list, &prev_rq_node, &next_rq_node);

	/* Create sorted list of requests */
	if(list_empty(&step->wls_trace_rqs)) {
		while(rq_count != 0) {
			rq = wl_create_request(step->wls_workload, NULL);
			tp_insert_request(rq_list, &rq->rq_node, &prev_rq_node, &next_rq_node, rq_node);
			--rq_count;
		}
	}
	else {
		list_for_each_entry_safe(request_t, rq, next_rq, &step->wls_trace_rqs, rq_node) {
			list_del(&rq->rq_node);

			rq_chain = rq;
			do {
				wl_hold(rq_chain->rq_workload);
				rq_chain = rq_chain->rq_chain_next;
			} while(rq_chain != NULL);

			tp_insert_request(rq_list, &rq->rq_node, &prev_rq_node, &next_rq_node, rq_node);
			++step->wls_rq_count;
		}
	}
}

tsobj_node_t* tsobj_tp_format(hm_item_t* object) {
	tsobj_node_t* node = NULL;
	tsobj_node_t* wl_list = NULL;
	tsobj_node_t* jwl;

	workload_t* wl;
	thread_pool_t* tp = (thread_pool_t*) object;

	/* TODO: Should return bindings && scheduler options */

	if(tp->tp_is_dead)
		return NULL;

	node = tsobj_new_node("tsload.ThreadPool");
	wl_list = tsobj_new_array();

	tsobj_add_integer(node, TSOBJ_STR("num_threads"), tp->tp_num_threads);
	tsobj_add_integer(node, TSOBJ_STR("quantum"), tp->tp_quantum);
	tsobj_add_integer(node, TSOBJ_STR("wl_count"), tp->tp_wl_count);

	tsobj_add_string(node, TSOBJ_STR("disp_name"),
					 tsobj_str_create(tp->tp_disp->tpd_class->name));

	mutex_lock(&tp->tp_mutex);
	list_for_each_entry(workload_t, wl, &tp->tp_wl_head, wl_tp_node) {
		tsobj_add_string(wl_list, NULL, wl->wl_name);
	}
	mutex_unlock(&tp->tp_mutex);

	tsobj_add_node(node, TSOBJ_STR("wl_list"), wl_list);

	return node;
}

/*
 * To check bindings externally:
 * Linux:
 * 		# taskset -p <pid>
 * Solaris (for single-strand bindings):
 * 		# pbind -Q
 * Windows:
 *		Attach windbg to a process
 *		> ~*
 *		Look at Affinity field
 * */

static int tp_bind_worker(thread_pool_t* tp, int wid, cpumask_t* binding) {
	tp_worker_t* worker = tp->tp_workers + wid;
	char* binding_str;

	int ret = sched_set_affinity(&worker->w_thread, binding);

	binding_str = worker_affinity_print(tp, wid);
	if(binding_str) {
		logmsg(LOG_INFO, "Worker #%d in threadpool '%s' was bound to %s",
				wid, tp->tp_name, binding_str);
		mp_free(binding_str);
	}
	else {
		logmsg(LOG_INFO, "Worker #%d in threadpool '%s' was bound",
				wid, tp->tp_name);
	}

	return ret;
}

static int tp_schedule_worker_commit(thread_pool_t* tp, int wid) {
	char* sched_str;
	tp_worker_t* worker = tp->tp_workers + wid;

	int err;

	err = sched_commit(&worker->w_thread);
	if(err != SCHED_OK)
		return err;

	sched_str = worker_sched_print(tp, wid);
	if(sched_str) {
		logmsg(LOG_INFO, "Worker #%d in threadpool '%s' was scheduled (%s)",
				wid, tp->tp_name, sched_str);
		mp_free(sched_str);
	}
	else {
		logmsg(LOG_INFO, "Worker #%d in threadpool '%s' was scheduled",
			    wid, tp->tp_name);
	}

	return SCHED_OK;
}

static int tsobj_tp_bind_worker(thread_pool_t* tp, int wid, tsobj_node_t* node) {
	tsobj_node_t* objects;
	tsobj_node_t* obj_node;
	int obj_id;

	char* obj_name;
	hi_cpu_object_t* object;

	int err = SCHED_ERROR;

	cpumask_t* binding;

	err = tsobj_get_array(node, "objects", &objects);
	if(err == TSOBJ_NOT_FOUND)
		return SCHED_OK;

	binding = cpumask_create();

	if(tsobj_size(objects) == 0) {
		hi_cpu_mask(NULL, binding);
	}
	else {
		tsobj_errno_clear();

		tsobj_for_each(objects, obj_node, obj_id) {
			obj_name = tsobj_as_string(obj_node);

			if(obj_name == NULL) {
				tsload_error_msg(tsobj_error_code(), TP_ERROR_SCHED_PREFIX "%s", tp->tp_name,
							     tsobj_error_message());
				goto end;
			}

			object = hi_cpu_find(obj_name);

			if(object == NULL) {
				tsload_error_msg(TSE_INVALID_VALUE, TP_ERROR_SCHED_PREFIX "no such object '%s'",
								 tp->tp_name, obj_name);
				goto end;
			}

			hi_cpu_mask(object, binding);
		}
	}

	err = tp_bind_worker(tp, wid, binding);

end:
	cpumask_destroy(binding);

	return err;
}

static int tsobj_tp_schedule_worker(thread_pool_t* tp, int wid, tsobj_node_t* node) {
	char* policy;
	int err;

	tp_worker_t* worker = tp->tp_workers + wid;

	tsobj_node_t* params;
	tsobj_node_t* param;
	int paramid;
	int64_t value;

	/* Set policy. If policy is not found, ignore scheduling */
	err = tsobj_get_string(node, "policy", &policy);
	if(err == TSOBJ_NOT_FOUND)
		return SCHED_OK;
	if(err != TSOBJ_OK)
		goto bad_tsobj;

	err = sched_set_policy(&worker->w_thread, policy);
	if(err != SCHED_OK)
		return err;

	/* Set policy params */
	err = tsobj_get_node(node, "params", &params);
	if(err == TSOBJ_INVALID_TYPE)
		goto bad_tsobj;

	if(err == JSON_OK) {
		tsobj_errno_clear();

		tsobj_for_each(params, param, paramid) {
			value = json_as_integer(param);

			if(tsobj_errno() != TSOBJ_OK)
				goto bad_tsobj;

			err = sched_set_param(&worker->w_thread,
								  tsobj_name(param), value);

			if(err != SCHED_OK) {
				tsload_error_msg(TSE_INVALID_DATA,
								 TP_ERROR_SCHED_PREFIX "failed to set param '%s' error = %d",
								 tp->tp_name, tsobj_name(param), err);
				return 1;
			}
		}
	}

	return tp_schedule_worker_commit(tp, wid);

bad_tsobj:
	tsload_error_msg(tsobj_error_code(), TP_ERROR_SCHED_PREFIX "%s",
					 tp->tp_name, tsobj_error_message());
	return SCHED_ERROR;
}

int tsobj_tp_schedule(thread_pool_t* tp, tsobj_node_t* sched) {
	int wid;
	int err;

	json_node_t* worker_sched;
	int id = -1;

	if(json_check_type(sched, JSON_ARRAY) != JSON_OK)
		goto bad_tsobj;

	tsobj_for_each(sched, worker_sched, id) {
		if(json_check_type(worker_sched, JSON_NODE) != JSON_OK)
			goto bad_tsobj_sched;

		if(json_get_integer_i(worker_sched, "wid", &wid) != JSON_OK) {
			goto bad_tsobj_sched;
		}

		if(wid >= tp->tp_num_threads || wid < 0) {
			tsload_error_msg(TSE_INVALID_VALUE,
							 TP_ERROR_SCHED_PREFIX "element #%d: invalid worker id #%d", tp->tp_name, wid);
			return 1;
		}

		if((err = tsobj_tp_bind_worker(tp, wid, worker_sched)) != 0) {
			/* XXX: revert previous schedule options?  */
			tsload_error_msg(TSE_INTERNAL_ERROR,
							 TP_ERROR_SCHED_PREFIX "couldn't bind worker #%d: error %d",
							 tp->tp_name, wid, err);
			return err;
		}

		if((err = tsobj_tp_schedule_worker(tp, wid, worker_sched)) != 0) {
			tsload_error_msg(TSE_INTERNAL_ERROR,
							 TP_ERROR_SCHED_PREFIX "couldn't schedule worker #%d: error %d",
							 tp->tp_name, wid, err);
			return err;
		}
	}

	return SCHED_OK;

bad_tsobj:
	tsload_error_msg(tsobj_error_code(), TP_ERROR_SCHED_PREFIX "%s",
					 tp->tp_name, tsobj_error_message());
	return SCHED_ERROR;

bad_tsobj_sched:
	tsload_error_msg(tsobj_error_code(), TP_ERROR_SCHED_PREFIX "element #%d: %s",
					 tp->tp_name, id, tsobj_error_message());
	return SCHED_ERROR;
}

static int tp_collect_tp(hm_item_t* item, void* arg) {
	thread_pool_t* tp = (thread_pool_t*) item;

	if(tp->tp_is_dead && atomic_read(&tp->tp_ref_count) == 0l) {
		tp_destroy_impl(tp, B_FALSE);
		return HM_WALKER_REMOVE | HM_WALKER_CONTINUE;
	}

	return HM_WALKER_CONTINUE;
}

static void tp_collect(void) {
	mutex_lock(&tp_collect_mutex);

	while(tp_collector_enabled || tp_count > 0) {
		mutex_unlock(&tp_collect_mutex);

		hash_map_walk(&tp_hash_map, tp_collect_tp, NULL);

		mutex_lock(&tp_collect_mutex);
		cv_wait_timed(&tp_collect_cv, &tp_collect_mutex, tp_collector_interval);
	}

	mutex_unlock(&tp_collect_mutex);
}

static thread_result_t tp_collector_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, void, unused);
	tp_collect();

THREAD_END:
	THREAD_FINISH(arg);
}

int tp_init(void) {
	tuneit_set_int(int, tp_max_threads);
	if(tp_max_threads < 1) {
		tp_max_threads = TPMAXTHREADS;
	}

	tuneit_set_int(ts_time_t, tp_min_quantum);
	tuneit_set_int(ts_time_t, tp_max_quantum);

	tuneit_set_int(ts_time_t, tp_collector_interval);
	tuneit_set_bool(tp_collector_enabled);

	tuneit_set_int(ts_time_t, tp_worker_min_sleep);
	tuneit_set_int(ts_time_t, tp_worker_overhead);

	mutex_init(&tp_collect_mutex, "tp_collect_mutex");
	cv_init(&tp_collect_cv, "tp_collect_cv");

	hash_map_init(&tp_hash_map, "tp_hash_map");

	mp_cache_init(&tp_cache, thread_pool_t);
	mp_cache_init(&tp_worker_cache, tp_worker_t);

	if(tp_collector_enabled) {
		t_init(&t_tp_collector, NULL, tp_collector_thread, "tp_collector");
	}

	return 0;
}

void tp_fini(void) {
	if(tp_collector_enabled) {
		mutex_lock(&tp_collect_mutex);
		tp_collector_enabled = B_FALSE;
		cv_notify_one(&tp_collect_cv);
		mutex_unlock(&tp_collect_mutex);

		t_destroy(&t_tp_collector);
	}
	else {
		tp_collect();
	}

	mp_cache_destroy(&tp_worker_cache);
	mp_cache_destroy(&tp_cache);

	hash_map_destroy(&tp_hash_map);

	cv_destroy(&tp_collect_cv);
	mutex_destroy(&tp_collect_mutex);
}

