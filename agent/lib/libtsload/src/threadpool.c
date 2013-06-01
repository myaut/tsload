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

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

mp_cache_t	   tp_cache;
mp_cache_t	   tp_worker_cache;

thread_pool_t* default_pool = NULL;

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

void* worker_thread(void* arg);
void* control_thread(void* arg);

static void tp_create_worker(thread_pool_t* tp, int tid) {
	tp_worker_t* worker = tp->tp_workers + tid;

	mutex_init(&worker->w_rq_mutex, "work-%s-%d", tp->tp_name, tid);

	list_head_init(&worker->w_requests, "rqs-%s-%d", tp->tp_name, tid);

	worker->w_tp = tp;

	t_init(&worker->w_thread, (void*) worker, worker_thread,
			"work-%s-%d", tp->tp_name, tid);
	worker->w_thread.t_local_id = WORKER_TID + tid;
}

static void tp_destroy_worker(thread_pool_t* tp, int tid) {
	tp_worker_t* worker = tp->tp_workers + tid;

	t_destroy(&worker->w_thread);

	mutex_destroy(&worker->w_rq_mutex);
}

/**
 * Create new thread pool
 *
 * @param num_threads Numbber of worker threads in this pool
 * @param name Name of thread pool
 * @param quantum Worker's quantum
 * */
thread_pool_t* tp_create(const char* name, unsigned num_threads, ts_time_t quantum, const char* disp_name) {
	thread_pool_t* tp = NULL;
	int tid;

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

	tp->tp_wl_count = 0;

	/*Initialize objects*/
	list_head_init(&tp->tp_wl_head, "tp-%s", name);
    event_init(&tp->tp_event, "tp-%s", name);
    mutex_init(&tp->tp_mutex, "tp-%s", name);

	/*Create threads*/
	t_init(&tp->tp_ctl_thread, (void*) tp, control_thread,
			"tp-ctl-%s", name);
	tp->tp_ctl_thread.t_local_id = CONTROL_TID;

	for(tid = 0; tid < num_threads; ++tid) {
		tp_create_worker(tp, tid);
	}

	hash_map_insert(&tp_hash_map, tp);

	logmsg(LOG_INFO, "Created thread pool %s with %d threads", name, num_threads);

	return tp;
}

static void tp_destroy_impl(thread_pool_t* tp) {
	int tid;
	workload_t *wl, *tmp;

	tp->tp_is_dead = B_TRUE;

	/* Destroy all workloads. No need to detach them,
	 * because we destroy entire threadpool */
	mutex_lock(&tp->tp_mutex);
	list_for_each_entry_safe(workload_t, wl, tmp, &tp->tp_wl_head, wl_tp_node) {
		wl_unconfig(wl);
		wl_destroy_nodetach(wl);
	}
	mutex_unlock(&tp->tp_mutex);

	/* Notify workers that we are done */
	event_notify_all(&tp->tp_event);

	for(tid = 0; tid < tp->tp_num_threads; ++tid) {
		tp_destroy_worker(tp, tid);
	}

	t_destroy(&tp->tp_ctl_thread);

	event_destroy(&tp->tp_event);
	mutex_destroy(&tp->tp_mutex);

	mp_cache_free_array(&tp_worker_cache, tp->tp_workers, tp->tp_num_threads);
	mp_cache_free(&tp_cache, tp);
}

void tp_destroy(thread_pool_t* tp) {
	hash_map_remove(&tp_hash_map, tp);

	tp_destroy_impl(tp);
}

thread_pool_t* tp_search(const char* name) {
	return hash_map_find(&tp_hash_map, name);
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
}

void tp_detach_nolock(thread_pool_t* tp, struct workload* wl) {
	wl->wl_tp = NULL;

	list_del(&wl->wl_tp_node);
	--tp->tp_wl_count;
	tp->tp_wl_changed = B_TRUE;
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
 * Helper for tp_distribute_requests
 *
 * Distributes num requests across array elements randomly
 *
 * @param def default number of requests per array item
 * @param num extra number of requests
 * @param len length of array
 * @param array pointer to array
 * */
static void distribute_requests(int def, int num, int len, int* array) {
	int i = 0, bit = 0;
	long rand_value = rand();

	/*Initialize array*/
	for(i = 0; i < len; ++i) {
		array[i] = def;
	}

	while(num > 0) {
		if(rand_value && (1 << bit)) {
			/* We found one, attach request*/
			--num;
			array[bit % len]++;
		}

		/* We exhausted rand_value, regenerate it*/
		if((1 << bit) >= RAND_MAX) {
			bit = 0;
			rand_value = rand();
		}

		bit++;
	}
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
	unsigned rq_per_worker = step->wls_rq_count / tp->tp_num_threads;
	unsigned extra_rqs = step->wls_rq_count % tp->tp_num_threads;

	int* num_rqs = NULL;
	request_t* rq;

	int tid;

	if(step->wls_rq_count == 0)
		return;

	num_rqs = (int*) mp_malloc(tp->tp_num_threads * sizeof(int));

	distribute_requests(rq_per_worker, extra_rqs, tp->tp_num_threads, num_rqs);

	for(tid = 0; tid < tp->tp_num_threads; ++tid) {
		while(num_rqs[tid] > 0) {
			rq = wl_create_request(step->wls_workload, tid);

			list_add_tail(&rq->rq_node, &tp->tp_workers[tid].w_requests);

			--num_rqs[tid];
		}
	}

	mp_free(num_rqs);
}

JSONNODE* json_tp_format(hm_item_t* object) {
	JSONNODE* node = json_new(JSON_NODE);
	JSONNODE* wl_list = json_new(JSON_ARRAY);
	JSONNODE* jwl;
	workload_t* wl;

	thread_pool_t* tp = (thread_pool_t*) object;

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

int tp_destroy_walk(hm_item_t* object, void* arg) {
	thread_pool_t* tp = (thread_pool_t*) object;

	tp_destroy_impl(tp);

	return HM_WALKER_CONTINUE | HM_WALKER_REMOVE;
}

int tp_init(void) {
	hash_map_init(&tp_hash_map, "tp_hash_map");

	mp_cache_init(&tp_cache, thread_pool_t);
	mp_cache_init(&tp_worker_cache, tp_worker_t);

	/*FIXME: default pool should have threads number num_of_phys_cores*/
	default_pool = tp_create(DEFAULT_TP_NAME, 4, T_SEC, "simple");

	return 0;
}

void tp_fini(void) {
	mp_cache_destroy(&tp_worker_cache);
	mp_cache_destroy(&tp_cache);

	hash_map_walk(&tp_hash_map, tp_destroy_walk, NULL);
	hash_map_destroy(&tp_hash_map);
}
