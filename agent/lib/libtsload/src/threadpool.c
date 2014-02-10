/*
 * threadpool.c
 *
 *  Created on: 10.11.2012
 *      Author: myaut
 */


#define LOG_SOURCE "tpool"
#include <log.h>

#include <hashmap.h>
#include <mempool.h>
#include <threads.h>
#include <threadpool.h>
#include <defs.h>
#include <workload.h>
#include <cpuinfo.h>
#include <cpumask.h>
#include <schedutil.h>
#include <tsload.h>
#include <list.h>
#include <tpdisp.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

mp_cache_t	   tp_cache;
mp_cache_t	   tp_worker_cache;

DECLARE_HASH_MAP(tp_hash_map, thread_pool_t, TPHASHSIZE, tp_name, tp_next,
	{
		return hm_string_hash(key, TPHASHMASK);
	},
	{
		return strcmp((char*) key1, (char*) key2) == 0;
	}
)

/* Minimum quantum duration. Should be set to system's tick
 * Maximum reasonable quantum duration. Currently 10 min*/
ts_time_t tp_min_quantum = TP_MIN_QUANTUM;
ts_time_t tp_max_quantum = TP_MAX_QUANTUM;

boolean_t tp_collector_enabled = B_TRUE;
ts_time_t tp_collector_interval = T_SEC / 2;

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

	if(num_threads > TPMAXTHREADS || num_threads == 0) {
		logmsg(LOG_WARN, "Failed to create thread_pool %s: maximum %d threads allowed (%d requested)",
				name, TPMAXTHREADS, num_threads);
		return NULL;
	}

	if(quantum < tp_min_quantum) {
		logmsg(LOG_WARN, "Failed to create thread_pool %s: too small quantum %lld (%lld minimum)",
						name, quantum, tp_min_quantum);
		return NULL;
	}

	tp = (thread_pool_t*) mp_cache_alloc(&tp_cache);

	strncpy(tp->tp_name, name, TPNAMELEN);

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

	for(tid = 0; tid < tp->tp_num_threads; ++tid) {
		tp_destroy_worker(tp, tid);
	}

	if(tp->tp_started)
		t_destroy(&tp->tp_ctl_thread);

	tp->tp_discard = B_TRUE;
	tp->tp_disp->tpd_class->control_report(tp);
	tpd_destroy(tp->tp_disp);

	mutex_destroy(&tp->tp_mutex);

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
	request_t* prev_rq = NULL;

	list_node_t* prev_rq_node = NULL;
	list_node_t* next_rq_node = NULL;

	if(!list_empty(rq_list)) {
		prev_rq = list_first_entry(request_t, rq_list, rq_node);

		prev_rq_node = &prev_rq->rq_node;

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
 * Distribute requests across threadpool's workers
 *
 * I.e. we have to distribute 11 requests across 4 workers
 * First of all, it attaches 2 rqs (10 / 4 = 2 for integer division)
 * to each worker then distributes 3 rqs randomly
 *
 * @note Called with w_rq_mutex held for all workers*/
void tp_distribute_requests(workload_step_t* step, thread_pool_t* tp) {
	unsigned rq_count = step->wls_rq_count;

	request_t* rq;
	request_t* prev_rq = NULL;
	request_t* next_rq = NULL;

	list_head_t* rq_list = &tp->tp_rq_head;
	list_node_t* prev_rq_node = NULL;
	list_node_t* next_rq_node = NULL;

	int tid;

	if(step->wls_rq_count == 0)
		return;

	tp_insert_request_initnodes(rq_list, &prev_rq_node, &next_rq_node);

	/* Create sorted list of requests */
	while(rq_count != 0) {
		rq = wl_create_request(step->wls_workload, NULL);

		tp_insert_request(rq_list, &rq->rq_node, &prev_rq_node, &next_rq_node, rq_node);

		--rq_count;
	}
}

JSONNODE* json_tp_format(hm_item_t* object) {
	JSONNODE* node = NULL;
	JSONNODE* wl_list = NULL;
	JSONNODE* jwl;
	workload_t* wl;

	thread_pool_t* tp = (thread_pool_t*) object;

	/* TODO: Should return bindings && scheduler options */

	if(tp->tp_is_dead)
		return NULL;

	node = json_new(JSON_NODE);
	wl_list = json_new(JSON_ARRAY);

	json_push_back(node, json_new_i("num_threads", tp->tp_num_threads));
	json_push_back(node, json_new_i("quantum", tp->tp_quantum));
	json_push_back(node, json_new_i("wl_count", tp->tp_wl_count));
	json_push_back(node, json_new_a("disp_name", "simple"));

	mutex_lock(&tp->tp_mutex);
	list_for_each_entry(workload_t, wl, &tp->tp_wl_head, wl_tp_node) {
		jwl = json_new(JSON_STRING);
		json_set_a(jwl, wl->wl_name);
		json_push_back(wl_list, jwl);
	}
	mutex_unlock(&tp->tp_mutex);

	json_set_name(node, tp->tp_name);
	json_set_name(wl_list, "wl_list");

	json_push_back(node, wl_list);

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

static int json_tp_bind_worker(thread_pool_t* tp, int wid, JSONNODE* node) {
	JSONNODE_ITERATOR i_objects;
	JSONNODE_ITERATOR i_object;
	JSONNODE_ITERATOR i_objects_end;
	JSONNODE_ITERATOR i_end;

	char* obj_name;
	hi_cpu_object_t* object;
	tp_worker_t* worker = tp->tp_workers + wid;

	int err = 0;

	cpumask_t* binding;

	char* binding_str;

	i_end = json_end(node);
	i_objects = json_find(node, "objects");

	if(i_objects == i_end) {
		return 0;
	}

	if(json_type(*i_objects) != JSON_ARRAY) {
		tsload_error_msg(TSE_MESSAGE_FORMAT,
						 TP_ERROR_SCHED_PREFIX ": 'objects' element should be JSON_ARRAY", tp->tp_name);
		return 1;
	}

	binding = cpumask_create();

	i_objects_end = json_end(*i_objects);
	i_object = json_begin(*i_objects);

	if(i_objects_end == i_object) {
		hi_cpu_mask(NULL, binding);
	}
	else {
		for( ; i_object != i_objects_end ; ++i_object) {
			obj_name = json_as_string(*i_object);
			object = hi_cpu_find(obj_name);

			if(object == NULL) {
				tsload_error_msg(TSE_INVALID_DATA,
								 TP_ERROR_SCHED_PREFIX ": no such object '%s'", tp->tp_name, obj_name);
				json_free(obj_name);
				goto end;
			}

			hi_cpu_mask(object, binding);
		}
	}

	sched_set_affinity(&worker->w_thread, binding);

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

end:
	cpumask_destroy(binding);

	return err;
}

static int json_tp_schedule_worker(thread_pool_t* tp, int wid, JSONNODE* node) {
	char* policy;
	char* param;
	int64_t value;
	int err;
	char* sched_str;

	tp_worker_t* worker = tp->tp_workers + wid;

	JSONNODE_ITERATOR i_end, i_policy, i_param, i_params, i_params_end;

	i_end = json_end(node);
	i_policy = json_find(node, "policy");
	i_params = json_find(node, "params");

	if(i_policy == i_end) {
		return 0;
	}

	if(json_type(*i_policy) != JSON_STRING) {
		tsload_error_msg(TSE_MESSAGE_FORMAT,
						 TP_ERROR_SCHED_PREFIX ": 'policy' should be JSON_STRING", tp->tp_name);
		return 1;
	}

	policy = json_as_string(*i_policy);
	err = sched_set_policy(&worker->w_thread, policy);
	json_free(policy);

	if(err != SCHED_OK)
		return err;

	if(i_params != i_end) {
		if(json_type(*i_params) != JSON_NODE) {
			tsload_error_msg(TSE_MESSAGE_FORMAT,
							 TP_ERROR_SCHED_PREFIX ": 'params' should be JSON_NODE", tp->tp_name);
			return 1;
		}

		for(i_param = json_begin(*i_params),
				i_params_end = json_end(*i_params) ;
				i_param != i_params_end ;
				++i_param) {
			if(json_type(*i_param) != JSON_NUMBER) {
				tsload_error_msg(TSE_MESSAGE_FORMAT,
								 TP_ERROR_SCHED_PREFIX ": 'param' should be JSON_NUMBER", tp->tp_name);
				return 1;
			}

			value = json_as_int(*i_param);
			param = json_name(*i_param);

			err = sched_set_param(&worker->w_thread, param, value);

			if(err != SCHED_OK) {
				tsload_error_msg(TSE_INVALID_DATA,
								 TP_ERROR_SCHED_PREFIX ": failed to set param '%s' error = %d",
								 tp->tp_name, param, err);
				json_free(param);
				return 1;
			}

			json_free(param);
		}
	}

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

	return 0;
}

int json_tp_schedule(thread_pool_t* tp, JSONNODE* sched) {
	JSONNODE_ITERATOR i_sched;
	JSONNODE_ITERATOR i_end;
	JSONNODE_ITERATOR i_wid, i_w_end;

	int wid;
	int err;

	if(json_type(sched) != JSON_ARRAY) {
		tsload_error_msg(TSE_MESSAGE_FORMAT,
						 TP_ERROR_SCHED_PREFIX ": 'sched' should be JSON_ARRAY", tp->tp_name);
		return 1;
	}

	i_end = json_end(sched);
	for(i_sched = json_begin(sched);
		i_sched != i_end ; ++i_sched) {
			if(json_type(*i_sched) != JSON_NODE) {
				tsload_error_msg(TSE_MESSAGE_FORMAT,
							     TP_ERROR_SCHED_PREFIX ": 'sched' element should be JSON_NODE",
								 tp->tp_name);
				return 1;
			}

			i_w_end = json_end(*i_sched);
			i_wid = json_find(*i_sched, "wid");

			if(i_wid == i_w_end) {
				tsload_error_msg(TSE_MESSAGE_FORMAT,
								 TP_ERROR_SCHED_PREFIX ": missing 'wid' ");
				return 1;
			}

			if(json_type(*i_wid) != JSON_NUMBER) {
				tsload_error_msg(TSE_MESSAGE_FORMAT,
								 TP_ERROR_SCHED_PREFIX ": 'wid' should be JSON_NUMBER");
				return 1;
			}

			wid = json_as_int(*i_wid);
			if(wid >= tp->tp_num_threads || wid < 0) {
				tsload_error_msg(TSE_INVALID_DATA,
							     TP_ERROR_SCHED_PREFIX ", invalid worker id #%d", tp->tp_name, wid);
				return 1;
			}

			if((err = json_tp_bind_worker(tp, wid, *i_sched)) != 0) {
				/* XXX: revert previous schedule options?  */
				tsload_error_msg(TSE_INVALID_DATA,
								 TP_ERROR_SCHED_PREFIX "  - couldn't bind worker #%d", tp->tp_name, wid);
				return err;
			}

			if((err = json_tp_schedule_worker(tp, wid, *i_sched)) != 0) {
				tsload_error_msg(TSE_INVALID_DATA,
								 TP_ERROR_SCHED_PREFIX "  - couldn't schedule worker #%d", tp->tp_name, wid);
				return err;
			}
	}

	return 0;
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
		tp_collector_enabled = B_FALSE;
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
