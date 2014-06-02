
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



#define LOG_SOURCE "workload"
#include <log.h>

#include <defs.h>
#include <list.h>
#include <hashmap.h>
#include <mempool.h>
#include <modules.h>
#include <threads.h>
#include <threadpool.h>
#include <workload.h>
#include <wltype.h>
#include <tsload.h>
#include <tstime.h>
#include <etrace.h>
#include <rqsched.h>
#include <tuneit.h>

#include <libjson.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

squeue_t	wl_requests;
squeue_t	wl_notifications;

thread_t	t_wl_requests;
thread_t	t_wl_notify;

mp_cache_t	wl_cache;
mp_cache_t	wl_rq_cache;

#define	WL_SET_STATUS(wl, status)					\
	do {											\
		(wl)->wl_status = status;					\
		(wl)->wl_status_flags |= (1 << status);		\
	} while(0)

#define	WL_HAD_STATUS(wl, status)							\
	TO_BOOLEAN((wl)->wl_status_flags & (1 << status))

/**
 * TSLoad supports statically-defined userspace dynamic tracing (USDT) using etrace subsystem.
 * Build with --enable-usdt (Linux + SystemTap or Solaris + DTrace) or --enable-etw
 * (Event Tracing for Windows).
 *
 * __Windows__:
 *		* Register provider with wevtutil:
 *			`C:\> wevtutil im Shared\workload.man`
 *		* Start tracing with logman:
 *		 	`C:\> logman start -ets TSloadWL -p "Tslaod-Workload" 0 0 -o tracelog.etl`
 *		* Run tsexperiment:
 *			`C:\> tsexperiment -e Data\sample\ run`
 *		* Stop tracing:
 *			`C:\> logman stop -ets TSloadWL`
 *		* Generate reports:
 *			`C:\> tracerpt -y tracelog.etl`
 *
 *		* If you need to uninstall provider, call wevtutil with um command:
 *			`C:\> wevtutil um Shared\workload.man`
 *
 * __Linux__:
 *
 * ```
 * 		root@centos# stap -e '
 * 			probe process("../lib/libtsload.so").provider("tsload__workload").mark("request__start") {
 * 				println(user_string(@cast($arg1, "workload_t")->wl_name));
 * 			} ' \
 * 				-c 'tsexperiment -e ./var/tsload/sample run'
 * ```
 *
 * __Solaris__:
 *
 * ```
 * 		root@sol11# dtrace -n '
 * 			request-start {
 * 				this->wl_name = (char*) copyin(arg0, 64);
 * 				trace(stringof(this->wl_name));
 * 			}'	\
 * 				 -c 'tsexperiment -e ./var/tsload/sample run'
 * ```
 *
 * */

#define ETRC_GUID_TSLOAD_WORKLOAD	{0x9028d325, 0xfcfd, 0x49fd, {0xae, 0x39, 0x64, 0xbe, 0x46, 0xd2, 0x78, 0x42}}

ETRC_DEFINE_PROVIDER(tsload__workload, ETRC_GUID_TSLOAD_WORKLOAD);

ETRC_DEFINE_EVENT(tsload__workload, request__start, 1);
ETRC_DEFINE_EVENT(tsload__workload, request__finish, 2);
ETRC_DEFINE_EVENT(tsload__workload, workload__step, 3);

DECLARE_HASH_MAP(workload_hash_map, workload_t, WLHASHSIZE, wl_name, wl_hm_next,
	{
		return hm_string_hash(key, WLHASHMASK);
	},
	{
		return strcmp((char*) key1, (char*) key2) == 0;
	}
)

static atomic_t wl_count = (atomic_t) 0l;
ts_time_t wl_poll_interval = 200 * T_MS;

extern struct rqsched_class simple_rqsched_class;

/**
 * wl_create - create new workload: allocate memory and initialize fields
 *
 * @return NULL if malloc had failed or new workload object
 */
workload_t* wl_create(const char* name, wl_type_t* wlt, thread_pool_t* tp) {
	workload_t* wl = (workload_t*) mp_cache_alloc(&wl_cache);
	workload_step_t* step;
	int i;

	if(wl == NULL)
		return NULL;

	strncpy(wl->wl_name, name, WLNAMELEN);
	wl->wl_type = wlt;

	wl->wl_tp = tp;

	wl->wl_current_rq = 0;
	wl->wl_current_step = -1;
	wl->wl_last_step = -1;

	for(i = 0; i < WLSTEPQSIZE; ++i) {
		step = wl->wl_step_queue + i;

		step->wls_workload = wl;
		step->wls_rq_count = 0u;
		list_head_init(&step->wls_trace_rqs, "wl-trace-%d", name);
	}

	wl->wl_hm_next = NULL;
	wl->wl_chain_next = NULL;

	list_head_init(&wl->wl_requests, "wl-rq-%s", name);

	list_node_init(&wl->wl_tp_node);

	wl->wl_params = mp_malloc(wlt->wlt_params_size);
	wl->wl_private = NULL;

	wl->wl_status_flags = 0;
	WL_SET_STATUS(wl, WLS_NEW);

	wl->wl_start_time = TS_TIME_MAX;
	wl->wl_notify_time = 0;
	wl->wl_start_clock = TS_TIME_MAX;
	wl->wl_time = 0;

	wl->wl_rqsched_class = NULL;
	wl->wl_rqsched_private = NULL;

	mutex_init(&wl->wl_rq_mutex, "wl-%s-rq", name);
	mutex_init(&wl->wl_status_mutex, "wl-%s-st", name);
	mutex_init(&wl->wl_step_mutex, "wl-%s-step", name);
	wl->wl_ref_count = (atomic_t) 0ul;

	list_head_init(&wl->wl_wlpgen_head, "wl-%s-wlpgen", name);

	hash_map_insert(&workload_hash_map, wl);
	atomic_inc(&wl_count);

	wl_hold(wl);

	return wl;
}

/**
 * wl_destroy_nodetach - workload destructor
 *
 * */
void wl_destroy_impl(workload_t* wl) {
	int i;
	request_t* rq;
	request_t* rq_next;

	if(wl->wl_rqsched_class)
		wl->wl_rqsched_class->rqsched_fini(wl);

	if(WL_HAD_STATUS(wl, WLS_CONFIGURING))
		t_destroy(&wl->wl_cfg_thread);

	if(wl->wl_tp != NULL) {
		tp_detach(wl->wl_tp, wl);
	}

	if(wl->wl_chain_next) {
		wl_rele(wl->wl_chain_next);
	}

	for(i = 0; i < WLSTEPQSIZE; ++i) {
		wl_destroy_request_list(&(wl->wl_step_queue[i].wls_trace_rqs));
	}

	wl_notify(wl, WLS_DESTROYED, 0, "status flags: %d", wl->wl_status_flags);

	wlpgen_destroy_all(wl);

	mutex_destroy(&wl->wl_rq_mutex);
	mutex_destroy(&wl->wl_status_mutex);
	mutex_destroy(&wl->wl_step_mutex);

	mp_free(wl->wl_params);
	mp_cache_free(&wl_cache, wl);

	atomic_dec(&wl_count);
}

/**
 * wl_destroy - destroy workload
 * */
void wl_destroy(workload_t* wl) {
	mutex_lock(&wl->wl_status_mutex);
	WL_SET_STATUS(wl, WLS_DESTROYED);
	mutex_unlock(&wl->wl_status_mutex);

	hash_map_remove(&workload_hash_map, wl);

	wl_rele(wl);
}

workload_t* wl_search(const char* name) {
	return hash_map_find(&workload_hash_map, name);
}

void wl_hold(workload_t* wl) {
	atomic_inc(&wl->wl_ref_count);
}

void wl_rele(workload_t* wl) {
	boolean_t is_destroyed = B_FALSE;

	if(atomic_dec(&wl->wl_ref_count) == 1l) {
		mutex_lock(&wl->wl_status_mutex);
		is_destroyed = WL_HAD_STATUS(wl, WLS_DESTROYED);
		mutex_unlock(&wl->wl_status_mutex);

		if(is_destroyed) {
			wl_destroy_impl(wl);
		}
	}

	if(!is_destroyed) {
		assert(wl->wl_ref_count >= 0);
	}
}

/**
 * Finish workload - notify that it was finished
 */
void wl_finish(workload_t* wl) {
	long step = wl->wl_current_step;

	wl_notify(wl, WLS_FINISHED, wl->wl_current_step, "");
}

/**
 * Chain workload to backwards of request chain
 *
 * @param parent one of workloads in chain
 * @param wl workload that needs to be chained
 */
void wl_chain_back(workload_t* parent, workload_t* wl) {
	workload_t* root;

	while(parent->wl_chain_next != NULL) {
		parent = parent->wl_chain_next;
	}

	wl_hold(wl);

	parent->wl_chain_next = wl;

	mutex_lock(&wl->wl_status_mutex);
	WL_SET_STATUS(wl, WLS_CHAINED);
	mutex_unlock(&wl->wl_status_mutex);
}

/**
 * Notify server of workload configuring progress.
 *
 * done is presented in percent. If status is WLS_FAIL - it would be set to -1,
 * if status is WLS_SUCCESS or WLS_FINISHED - it would be set to 100
 *
 * @param wl configuring workload
 * @param status configuration/execution status
 * @param done configuration progress (in percent)
 * @param format message format string
 */
void wl_notify(workload_t* wl, wl_status_t status, long progress, char* format, ...) {
	wl_notify_msg_t* msg = NULL;
	va_list args;

	ts_time_t now = tm_get_time();

	/* Ignore intermediate progress messages if they are going too fast */
	if( status == WLS_CONFIGURING && progress > 2 && progress < 98 &&
		(now - wl->wl_notify_time) < (T_SEC / WL_NOTIFICATIONS_PER_SEC)) {
		return;
	}

	wl->wl_notify_time = now;

	switch(status) {
	case WLS_CONFIGURING:
		/* 146% and -5% are awkward */
		if(progress < 0) progress = 0;
		if(progress > 100) progress = 100;
	break;
	case WLS_CFG_FAIL:
		progress = -1;
	break;
	case WLS_CONFIGURED:
		progress = 100;
	break;
	case WLS_RUNNING:
	case WLS_FINISHED:
	case WLS_DESTROYED:
		progress = wl->wl_current_step;
		break;
	default:
		assert(status == WLS_CONFIGURING);
	break;
	}

	mutex_lock(&wl->wl_status_mutex);
	WL_SET_STATUS(wl, status);
	mutex_unlock(&wl->wl_status_mutex);

	msg = mp_malloc(sizeof(wl_notify_msg_t));

	va_start(args, format);
	vsnprintf(msg->msg, WLNOTIFYMSGLEN, format, args);
	va_end(args);

	logmsg((status == WLS_CFG_FAIL)? LOG_WARN : LOG_INFO,
			"Workload %s status: %s", wl->wl_name, msg->msg);

	strcpy(msg->wl_name, wl->wl_name);
	msg->status = status;
	msg->progress = progress;

	squeue_push(&wl_notifications, msg);
}

thread_result_t wl_notification_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, void, unused);

	wl_notify_msg_t* msg = NULL;

	while(B_TRUE) {
		msg = (wl_notify_msg_t*) squeue_pop(&wl_notifications);

		if(msg == NULL) {
			THREAD_EXIT(0);
		}

		tsload_workload_status(msg->wl_name, msg->status, msg->progress, msg->msg);

		mp_free(msg);
	}

THREAD_END:
	THREAD_FINISH(arg);
}

thread_result_t wl_config_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, workload_t, wl);
	int ret;

	wl_notify(wl, WLS_CONFIGURING, 0, "");

	ret = wl->wl_type->wlt_wl_config(wl);

	if(ret != 0) {
		logmsg(LOG_WARN, "Failed to configure workload %s", wl->wl_name);

		THREAD_EXIT(ret);
	}

	if(wl->wl_tp) {
		tp_attach(wl->wl_tp, wl);
	}

	wl_notify(wl, WLS_CONFIGURED, 100, "");


THREAD_END:
	THREAD_FINISH(arg);
}

void wl_config(workload_t* wl) {
	t_init(&wl->wl_cfg_thread, (void*) wl, wl_config_thread,
			"wl-cfg-%s", wl->wl_name);
}

void wl_unconfig(workload_t* wl) {
	wl->wl_type->wlt_wl_unconfig(wl);
}

int wl_is_started(workload_t* wl) {
	boolean_t ret = B_FALSE;

	/* fastpath - not lock nutex */
	if(WL_HAD_STATUS(wl, WLS_STARTED))
		return B_TRUE;

	mutex_lock(&wl->wl_status_mutex);
	if(wl->wl_status == WLS_CONFIGURED &&
	   tm_get_time() >= wl->wl_start_time) {
		logmsg(LOG_INFO, "Starting workload %s...", wl->wl_name);

		WL_SET_STATUS(wl, WLS_STARTED);
		ret = B_TRUE;
	}
	mutex_unlock(&wl->wl_status_mutex);

	return ret;
}

/**
 * Provide number of requests for current step
 *
 * @param wl workload
 * @param step_id step id to be provided
 * @param num_rqs number
 * @param trace_rqs linked list of trace-based request
 *
 * @return 0 if steps are saved, -1 if incorrect step provided or wl_requests is full
 * */
int wl_provide_step(workload_t* wl, long step_id, unsigned num_rqs, list_head_t* trace_rqs) {
	int ret = WL_STEP_OK;
	long step_off = step_id & WLSTEPQMASK;
	workload_step_t* step = wl->wl_step_queue + step_off;

	mutex_lock(&wl->wl_step_mutex);

	/* Reserve 1 slot for current step because it may be
	 * still processing by control thread of threadpool */
	if((wl->wl_last_step - wl->wl_current_step + 1) == WLSTEPQSIZE) {
		ret = WL_STEP_QUEUE_FULL;
		goto done;
	}

	if(step_id != (wl->wl_last_step + 1)) {
		ret = WL_STEP_INVALID;
		goto done;
	}

	wl->wl_last_step = step_id;

	step->wls_rq_count = num_rqs;
	list_splice_init(trace_rqs, list_head_node(&step->wls_trace_rqs));

done:
	mutex_unlock(&wl->wl_step_mutex);

	return ret;
}

/**
 * Proceed to next step, and return number of requests in it
 *
 * @param wl Workload to work with.
 *
 * @return Workload's step or NULL of no steps are on queue
 * */
workload_step_t* wl_advance_step(workload_t* wl) {
	long step_off;
	workload_step_t* step = NULL;

	assert(wl != NULL);

	mutex_lock(&wl->wl_step_mutex);

	++wl->wl_current_step;
	wl->wl_current_rq = 0;

	if(wl->wl_current_step > wl->wl_last_step) {
		/* No steps on queue */
		logmsg(LOG_INFO, "No steps on queue %s, step #%ld!", wl->wl_name, wl->wl_current_step);
		wl_finish(wl);

		goto end;
	}

	ETRC_PROBE1(tsload__workload, workload__step, workload_t*, wl);

	step_off = wl->wl_current_step  & WLSTEPQMASK;
	step = wl->wl_step_queue + step_off;

	wl->wl_rqsched_class->rqsched_step(step);

end:
	mutex_unlock(&wl->wl_step_mutex);

	return step;
}

static void wl_init_request(workload_t* wl, request_t* rq) {
	rq->rq_thread_id = -1;

	rq->rq_flags = 0;

	rq->rq_workload = wl;

	rq->rq_sched_time = 0;
	rq->rq_start_time = 0;
	rq->rq_end_time = 0;

	rq->rq_queue_len = -1;

	list_node_init(&rq->rq_node);
	list_node_init(&rq->rq_w_node);
	list_node_init(&rq->rq_wl_node);
}


/**
 * Create request structure, append it to requests queue, initialize
 * For chained workloads inherits parent step and request id
 *
 * @param wl workload for request
 * @param parent parent request (for chained workloads) \
 * 		For unchained workloads should be set to NULL.
 * */
request_t* wl_create_request(workload_t* wl, request_t* parent) {
	request_t* rq = (request_t*) mp_cache_alloc(&wl_rq_cache);

	double u;

	wl_hold(wl);

	if(parent == NULL) {
		rq->rq_step = wl->wl_current_step;
		rq->rq_id = wl->wl_current_rq++;
		rq->rq_user_id = 0;
	}
	else {
		rq->rq_step = parent->rq_step;
		rq->rq_id = parent->rq_id;
		rq->rq_user_id = parent->rq_user_id;
	}

	wl_init_request(wl, rq);

	rq->rq_params = wlpgen_generate(wl);

	wl->wl_rqsched_class->rqsched_pre_request(rq);

	if(wl->wl_chain_next != NULL &&
			(wl->wl_chain_next->wl_chain_rg == NULL ||
			 rg_generate_double(wl->wl_chain_next->wl_chain_rg) >=
			 	 wl->wl_chain_next->wl_chain_probability)) {
		rq->rq_chain_next = wl_create_request(wl->wl_chain_next, rq);
	}
	else {
		rq->rq_chain_next = NULL;
	}

	mutex_lock(&wl->wl_rq_mutex);
	list_add_tail(&rq->rq_wl_node, &wl->wl_requests);
	mutex_unlock(&wl->wl_rq_mutex);

	logmsg(LOG_TRACE, "Created request %s/%d step: %d sched_time: %lld", wl->wl_name,
					rq->rq_id, rq->rq_step, (long long) rq->rq_sched_time);

	return rq;
}

/**
 * Clone request - used by benchmark threadpool dispatcher
 */
request_t* wl_clone_request(request_t* origin) {
	workload_t* wl = origin->rq_workload;
	request_t* rq = (request_t*) mp_cache_alloc(&wl_rq_cache);

	wl_hold(wl);

	rq->rq_step = origin->rq_step;
	rq->rq_id = wl->wl_current_rq++;
	rq->rq_user_id = origin->rq_user_id;

	wl_init_request(wl, rq);

	rq->rq_params = wlpgen_generate(wl);

	if(origin->rq_chain_next != NULL) {
		rq->rq_chain_next = wl_clone_request(origin->rq_chain_next);
	}
	else {
		rq->rq_chain_next = NULL;
	}

	mutex_lock(&wl->wl_rq_mutex);
	list_add_tail(&rq->rq_wl_node, &wl->wl_requests);
	mutex_unlock(&wl->wl_rq_mutex);

	logmsg(LOG_TRACE, "Cloned request %s/%d step: %d sched_time: %lld", wl->wl_name,
					rq->rq_id, rq->rq_step, (long long) rq->rq_sched_time);

	return rq;
}

/**
 * Create trace-based requests
 */
request_t* wl_create_request_trace(workload_t* wl, int rq_id, long step, int user_id, int thread_id,
								   ts_time_t sched_time, void* rq_params) {
	request_t* rq = (request_t*) mp_cache_alloc(&wl_rq_cache);

	wl_init_request(wl, rq);

	rq->rq_id = rq_id;
	rq->rq_step = step;
	rq->rq_user_id = user_id;
	rq->rq_thread_id = thread_id;

	rq->rq_sched_time = sched_time;

	rq->rq_params = mp_malloc(wl->wl_type->wlt_rqparams_size);
	memcpy(rq->rq_params, rq_params, wl->wl_type->wlt_rqparams_size);

	/* Let upper layer do it's job of creating chain requests */
	rq->rq_chain_next = NULL;

	mutex_lock(&wl->wl_rq_mutex);
	list_add_tail(&rq->rq_wl_node, &wl->wl_requests);
	mutex_unlock(&wl->wl_rq_mutex);

	rq->rq_flags = RQF_TRACE;

	logmsg(LOG_TRACE, "Created trace-based request %s/%d step: %d sched_time: %lld", wl->wl_name,
			rq->rq_id, rq->rq_step, (long long) rq->rq_sched_time);

	return rq;
}

/**
 * Destroy request memory */
void wl_request_destroy(request_t* rq) {
	logmsg(LOG_TRACE, "Destroyed request %s/%d step: %d thread: %d", rq->rq_workload->wl_name,
			rq->rq_id, rq->rq_step, rq->rq_thread_id);

	list_del(&rq->rq_wl_node);

	if(rq->rq_params != NULL) {
		mp_free(rq->rq_params);
	}

	wl_rele(rq->rq_workload);
	memset(rq, 0xba, sizeof(request_t));

	mp_cache_free(&wl_rq_cache, rq);
}

/**
 * Run request for execution */
void wl_run_request(request_t* rq) {
	int ret;
	workload_t* wl = rq->rq_workload;

	ETRC_PROBE2(tsload__workload, request__start, workload_t*, wl, request_t*, rq);

	logmsg(LOG_TRACE, "Running request %s/%d step: %d worker: #%d sched: %lld start: %lld",
		rq->rq_workload->wl_name, rq->rq_id, rq->rq_step, rq->rq_thread_id,
		(long long) rq->rq_sched_time, (long long) rq->rq_start_time);

	if(WL_HAD_STATUS(wl, WLS_FINISHED))
		return;

	rq->rq_flags |= RQF_STARTED;
	rq->rq_start_time = tm_get_clock() - wl->wl_start_clock;

	if(tm_diff(rq->rq_sched_time, rq->rq_start_time) > wl->wl_deadline) {
		return;
	}

	ret = wl->wl_type->wlt_run_request(rq);

	rq->rq_end_time = tm_get_clock() - wl->wl_start_clock;

	if(rq->rq_start_time <= rq->rq_sched_time)
		rq->rq_flags |= RQF_ONTIME;

	if(ret == 0)
		rq->rq_flags |= RQF_SUCCESS;

	rq->rq_flags |= RQF_FINISHED;

	if(!(rq->rq_flags & RQF_TRACE))
		wl->wl_rqsched_class->rqsched_post_request(rq);

	if(rq->rq_chain_next != NULL) {
		rq->rq_chain_next->rq_sched_time = rq->rq_end_time;
	}

	ETRC_PROBE2(tsload__workload, request__finish, workload_t*, wl, request_t*, rq);
}

void wl_report_requests(list_head_t* rq_list) {
	squeue_push(&wl_requests, rq_list);
}

void wl_destroy_request_list(list_head_t* rq_list) {
	request_t *rq_root, *rq, *rq_tmp, *rq_next;

	list_for_each_entry_safe(request_t, rq_root, rq_tmp, rq_list, rq_node) {
		rq = rq_root;
		do {
			rq_next = rq->rq_chain_next;
			wl_request_destroy(rq);
			rq = rq_next;
		} while(rq != NULL);
	}
}

static void wl_rq_list_destroy(void* p_rq_list) {
	list_head_t* rq_list = (list_head_t*) p_rq_list;

	wl_destroy_request_list(rq_list);
	mp_free(rq_list);
}

thread_result_t wl_requests_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, void, unused);
	list_head_t* rq_list;

	request_t *rq, *rq_tmp;

	while(B_TRUE) {
		rq_list =  (list_head_t*) squeue_pop(&wl_requests);

		if(rq_list == NULL) {
			THREAD_EXIT(0);
		}

		tsload_requests_report(rq_list);

		wl_rq_list_destroy(rq_list);
	}

THREAD_END:
	THREAD_FINISH(arg);
}

JSONNODE* json_request_format_all(list_head_t* rq_list) {
	JSONNODE* jrq;
	JSONNODE* j_rq_list = json_new(JSON_ARRAY);

	request_t* rq;

	list_for_each_entry(request_t, rq, rq_list, rq_node) {
		jrq = json_new(JSON_NODE);

		json_push_back(jrq, json_new_a("workload_name", rq->rq_workload->wl_name));

		json_push_back(jrq, json_new_i("step", rq->rq_step));
		json_push_back(jrq, json_new_i("request", rq->rq_id));
		json_push_back(jrq, json_new_i("thread", rq->rq_thread_id));

		json_push_back(jrq, json_new_i("sched", rq->rq_sched_time));
		json_push_back(jrq, json_new_i("start", rq->rq_start_time));
		json_push_back(jrq, json_new_i("end", rq->rq_end_time));

		json_push_back(jrq, json_new_i("flags", rq->rq_flags));

		json_push_back(j_rq_list, jrq);
	}

	return j_rq_list;
}

workload_t* json_workload_proc(const char* wl_name, const char* wl_type, const char* tp_name, ts_time_t deadline,
		                       JSONNODE* wl_chain_params, JSONNODE* rqsched_params, JSONNODE* wl_params) {

	workload_t* wl = NULL;
	workload_t* parent = NULL;
	wl_type_t* wlt = NULL;
	thread_pool_t* tp = NULL;

	char* wl_chain_name;
	double wl_chain_probability = -1.0;
	randgen_t* wl_chain_rg = NULL;

	JSONNODE_ITERATOR i_randgen, i_workload, i_probability,
					  i_prob_value, i_prob_end, i_end;

	int ret = RQSCHED_JSON_OK;

	wlt = wl_type_search(wl_type);

	if(wl_params == NULL) {
		tsload_error_msg(TSE_MESSAGE_FORMAT,
				         "Workload / request parameters was not provided for workload '%s'", wl_name);
		goto fail;
	}

	if(tp_name) {
		tp = tp_search(tp_name);

		if(rqsched_params == NULL) {
			tsload_error_msg(TSE_MESSAGE_FORMAT,
					         "Request scheduler parameters was not provided for workload '%s'", wl_name);
			goto fail;
		}
	}
	else if(wl_chain_params) {
		i_end = json_end(wl_chain_params);
		i_probability = json_find(wl_chain_params, "probability");
		i_workload = json_find(wl_chain_params, "workload");

		if(i_probability != i_end) {
			i_prob_end = json_end(*i_probability);
			i_randgen = json_find(*i_probability, "randgen");
			i_prob_value = json_find(*i_probability, "value");

			if(i_randgen != i_prob_end) {
				wl_chain_rg = json_randgen_proc(*i_randgen);
			}

			if(i_prob_value != i_prob_end) {
				wl_chain_probability = json_as_float(*i_prob_value);
			}

			if(wl_chain_probability < 0.0 || wl_chain_probability > 1.0) {
				tsload_error_msg(TSE_MESSAGE_FORMAT,
								 "Not specified or incorrect chain probability value for workload '%s'", wl_name);
				goto fail;
			}

			if(wl_chain_rg == NULL) {
				tsload_error_msg(TSE_MESSAGE_FORMAT,
								 "Not specified or incorrect random generator "
								 "of chain probability for workload '%s'", wl_name);
				goto fail;
			}
		}

		if(i_workload == i_end) {
			tsload_error_msg(TSE_INVALID_DATA,
							 "Not specified chain parent for workload '%s'", wl_name);
			goto fail;
		}

		wl_chain_name = json_as_string(*i_workload);
		parent = wl_search(wl_chain_name);

		if(parent == NULL) {
			tsload_error_msg(TSE_INVALID_DATA,
							 "Not found chain parent '%s' for workload '%s'", wl_chain_name, wl_name);
			json_free(wl_chain_name);
			goto fail;
		}

		json_free(wl_chain_name);
	}
	else {
		tsload_error_msg(TSE_INVALID_DATA,
						 "Neither threadpool nor chain are set for workload '%s'", wl_name);
		goto fail;
	}

	/* Get workload's name */
	logmsg(LOG_DEBUG, "Parsing workload %s", wl_name);

	/* Create workload */
	wl = wl_create(wl_name, wlt, tp);

	/* Process request scheduler.
	 * Chained workloads do not have scheduler - use simple scheduler */
	if(parent == NULL) {
		ret = json_rqsched_proc(rqsched_params, wl);
	}
	else {
		wl->wl_rqsched_class = &simple_rqsched_class;
	}

	if(ret != RQSCHED_JSON_OK) {
		goto fail;
	}

	/* Process params from i_params to wl_params, where mod_params contains
	 * parameters descriptions (see wlparam) */
	ret = json_wlparam_proc_all(wl_params, wlt->wlt_params, wl);

	if(ret != WLPARAM_JSON_OK) {
		goto fail;
	}

	/* Chain workload */
	if(parent != NULL) {
		wl_chain_back(parent, wl);

		wl->wl_chain_probability = wl_chain_probability;
		wl->wl_chain_rg = wl_chain_rg;
	}

	wl->wl_deadline = deadline;

	return wl;

fail:
	if(wl)
		wl_destroy(wl);

	return NULL;
}

int wl_init(void) {
	tuneit_set_int(ts_time_t, wl_poll_interval);

	hash_map_init(&workload_hash_map, "workload_hash_map");

	squeue_init(&wl_requests, "wl-requests");
	t_init(&t_wl_requests, NULL, wl_requests_thread, "wl_requests");

	squeue_init(&wl_notifications, "wl-notify");
	t_init(&t_wl_notify, NULL, wl_notification_thread, "wl_notification");

	mp_cache_init(&wl_rq_cache, request_t);
	mp_cache_init(&wl_cache, workload_t);

	etrc_provider_init(&tsload__workload);

	return 0;
}

void wl_fini(void) {
	etrc_provider_destroy(&tsload__workload);

	while(atomic_read(&wl_count) > 0l) {
		tm_sleep_milli(wl_poll_interval);
	}

	squeue_push(&wl_requests, NULL);
	t_destroy(&t_wl_requests);
	squeue_destroy(&wl_requests, wl_rq_list_destroy);

	squeue_push(&wl_notifications, NULL);
	t_destroy(&t_wl_notify);
	squeue_destroy(&wl_notifications, mp_free);

	hash_map_destroy(&workload_hash_map);

	mp_cache_destroy(&wl_rq_cache);
	mp_cache_destroy(&wl_cache);
}

