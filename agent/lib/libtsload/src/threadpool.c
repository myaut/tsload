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

/* Minimum quantum duration. Should be set to system's tick */
ts_time_t tp_min_quantum = TP_MIN_QUANTUM;

boolean_t tp_collector_enabled = B_TRUE;
ts_time_t tp_collector_interval = T_SEC / 2;

static thread_t  t_tp_collector;

static atomic_t tp_count = (atomic_t) 0l;

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

	atomic_inc(&tp_count);
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

	tpd_destroy(tp->tp_disp);

	if(tp->tp_started)
		t_destroy(&tp->tp_ctl_thread);

	mutex_destroy(&tp->tp_mutex);

	mp_cache_free_array(&tp_worker_cache, tp->tp_workers, tp->tp_num_threads);
	mp_cache_free(&tp_cache, tp);

	atomic_dec(&tp_count);
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
	request_t* last_rq;
	request_t* next_rq;

	list_head_t* rq_list = &tp->tp_rq_head;

	int tid;

	if(step->wls_rq_count == 0)
		return;

	/* Create sorted list of requests */
	if(list_empty(rq_list)) {
		last_rq = NULL;
	}
	else {
		last_rq = list_last_entry(request_t, rq_list, rq_node);
	}

	while(rq_count != 0) {
		rq = wl_create_request(step->wls_workload, NULL);

		/* Usually there are only one workload per threadpool, so we may simple put
		 * new request after last request. But if it is second workload, these conditions
		 * would not be satisfied (there would be requests after current request, and
		 * we have to find appropriate place for it. */
		if(!last_rq ||
				(list_is_last(&last_rq->rq_node, rq_list) &&
				 rq->rq_sched_time >= last_rq->rq_sched_time) ) {
			list_add_tail(&rq->rq_node, rq_list);
			last_rq = rq;
		}
		else {
			boolean_t middle = B_FALSE;

			next_rq = last_rq;

			if(rq->rq_sched_time >= last_rq->rq_sched_time) {
				/* Search forward */
				list_for_each_entry_continue(request_t, next_rq, rq_list, rq_node) {
					if(rq->rq_sched_time < next_rq->rq_sched_time) {
						middle = B_TRUE;
						break;
					}
					last_rq = next_rq;
				}

				if(!middle) {
					list_add_tail(&rq->rq_node, rq_list);
				}
			}
			else {
				/* Search backward */
				list_for_each_entry_continue_reverse(request_t, last_rq, rq_list, rq_node) {
					if(rq->rq_sched_time >= last_rq->rq_sched_time) {
						middle = B_TRUE;
						break;
					}

					next_rq = last_rq;
				}

				if(!middle) {
					list_add(&rq->rq_node, rq_list);
				}
			}

			if(middle) {
				/* Insert new request between next_rq and last_rq */
				__list_add(&rq->rq_node, &last_rq->rq_node, &next_rq->rq_node);
			}
		}

		--rq_count;
	}
}

JSONNODE* json_tp_format(hm_item_t* object) {
	JSONNODE* node = NULL;
	JSONNODE* wl_list = NULL;
	JSONNODE* jwl;
	workload_t* wl;

	thread_pool_t* tp = (thread_pool_t*) object;

	/* TODO: Should return bindings */

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

JSONNODE* json_tp_format_all(void) {
	return json_hm_format_all(&tp_hash_map, json_tp_format);
}

#define JSON_GET_VALIDATE_PARAM(iter, tp_name, name, req_type)	\
	{											\
		iter = json_find(node, name);			\
		if(iter == i_end) {						\
			tsload_error_msg(TSE_MESSAGE_FORMAT,	\
					"Failed to parse threadpool %s, missing parameter %s", tp_name, name);	\
			return 1;							\
		}										\
		if(json_type(*iter) != req_type) {		\
			tsload_error_msg(TSE_MESSAGE_FORMAT,	\
					"Expected that " name " is " #req_type );	\
			return 1;							\
		}										\
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

static int json_tp_bind_worker(thread_pool_t* tp, JSONNODE* node) {
	JSONNODE_ITERATOR i_tid;
	JSONNODE_ITERATOR i_objects;
	JSONNODE_ITERATOR i_object;
	JSONNODE_ITERATOR i_objects_end;
	JSONNODE_ITERATOR i_end;

	char* obj_name;
	hi_cpu_object_t* object;
	int tid;

	int err = 0;

	cpumask_t* binding;

	char* binding_str;

	if(json_type(node) != JSON_NODE) {
		tsload_error_msg(TSE_MESSAGE_FORMAT, "Failed to bind threadpool %s, binding must be node",
								tp->tp_name);
		return 1;
	}

	i_end = json_end(node);

	JSON_GET_VALIDATE_PARAM(i_tid, tp->tp_name, "tid", JSON_NUMBER);
	JSON_GET_VALIDATE_PARAM(i_objects, tp->tp_name, "objects", JSON_ARRAY);

	tid = json_as_int(*i_tid);
	if(tid > tp->tp_num_threads || tid < 0) {
		tsload_error_msg(TSE_INVALID_DATA, "Failed to bind threadpool %s, invalid tid %d",
								tp->tp_name, tid);
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
				tsload_error_msg(TSE_INVALID_DATA, "Failed to bind threadpool %s: no such object '%s'",
												tp->tp_name, obj_name);
				json_free(obj_name);
				goto end;
			}

			hi_cpu_mask(object, binding);
		}
	}

	sched_set_affinity(&tp->tp_workers[tid].w_thread, binding);

	binding_str = worker_affinity_print(tp, tid);
	if(binding_str) {
		logmsg(LOG_INFO, "json_tp_bind_worker: tp: '%s' tid: %d binding: %s",
				tp->tp_name, tid, binding_str);
		mp_free(binding_str);
	}
	else {
		logmsg(LOG_INFO, "json_tp_bind_worker: tp: '%s' tid: %d binding: ???",
				tp->tp_name, tid);
	}

end:
	cpumask_destroy(binding);

	return err;
}

int json_tp_bind(thread_pool_t* tp, JSONNODE* bindings) {
	JSONNODE_ITERATOR i_binding;
	JSONNODE_ITERATOR i_end;

	if(json_type(bindings) != JSON_ARRAY) {
		tsload_error_msg(TSE_MESSAGE_FORMAT, "Failed to bind threadpool %s, 'bindings' must be array", tp->tp_name);
		return 1;
	}

	i_end = json_end(bindings);
	for(i_binding = json_begin(bindings);
		i_binding != i_end ; ++i_binding) {
			if(json_tp_bind_worker(tp, *i_binding) != 0) {
				/* XXX: revert previous bindings?  */
				return 1;
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
	while(tp_collector_enabled || atomic_read(&tp_count) > 0) {
		hash_map_walk(&tp_hash_map, tp_collect_tp, NULL);
		tm_sleep_milli(tp_collector_interval);
	}
}

static thread_result_t tp_collector_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, void, unused);
	tp_collect();

THREAD_END:
	THREAD_FINISH(arg);
}

int tp_init(void) {
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

		/* FIXME: May wait up to tp_collector_interval - implement via timed events */
		t_destroy(&t_tp_collector);
	}
	else {
		tp_collect();
	}

	mp_cache_destroy(&tp_worker_cache);
	mp_cache_destroy(&tp_cache);

	hash_map_destroy(&tp_hash_map);
}
