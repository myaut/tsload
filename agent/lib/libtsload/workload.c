
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
#include <tsload/log.h>

#include <tsload/defs.h>

#include <tsload/list.h>
#include <tsload/hashmap.h>
#include <tsload/mempool.h>
#include <tsload/time.h>
#include <tsload/etrace.h>
#include <tsload/tuneit.h>
#include <tsload/autostring.h>

#include <tsload/obj/obj.h>

#include <tsload/load/threadpool.h>
#include <tsload/load/workload.h>
#include <tsload/load/wltype.h>
#include <tsload.h>
#include <tsload/load/rqsched.h>

#include <errormsg.h>

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

DECLARE_HASH_MAP_STRKEY(workload_hash_map, workload_t, WLHASHSIZE, wl_name, wl_hm_next, WLHASHMASK);

static atomic_t wl_count = (atomic_t) 0l;
ts_time_t wl_poll_interval = 200 * T_MS;

extern tsload_workload_status_func tsload_workload_status;
extern tsload_requests_report_func tsload_requests_report;

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

	aas_copy(aas_init(&wl->wl_name), name);
	wl->wl_type = wlt;

	wl->wl_tp = tp;

	wl->wl_current_rq = 0;
	wl->wl_current_step = -1;
	wl->wl_last_step = -1;

	for(i = 0; i < WLSTEPQSIZE; ++i) {
		step = wl->wl_step_queue + i;

		step->wls_workload = wl;
		step->wls_rq_count = 0u;
		list_head_init(&step->wls_trace_rqs, "wl-trace-%s", name);
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
		rqsched_destroy(wl);

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

	wl_notify(wl, WLS_DESTROYED, 0, "status flags: %lx", wl->wl_status_flags);

	wlpgen_destroy_all(wl);

	mutex_destroy(&wl->wl_rq_mutex);
	mutex_destroy(&wl->wl_status_mutex);
	mutex_destroy(&wl->wl_step_mutex);

	aas_free(&wl->wl_name);

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
	wl_notify(wl, WLS_FINISHED, wl->wl_current_step, "%s", "");
}

/**
 * Stop workload - no more steps for this workload
 */
void wl_stop(workload_t* wl) {
    wl_notify(wl, WLS_STOPPED, wl->wl_current_step, "%s", "");
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
    case WLS_STOPPED:
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
	aas_vprintf(aas_init(&msg->msg), format, args);
	va_end(args);

	logmsg((status == WLS_CFG_FAIL)? LOG_WARN : LOG_INFO,
			"Workload %s status: %s", wl->wl_name, msg->msg);

	aas_copy(aas_init(&msg->wl_name), wl->wl_name);
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

		aas_free(&msg->wl_name);
		aas_free(&msg->msg);
		mp_free(msg);
	}

THREAD_END:
	THREAD_FINISH(arg);
}

thread_result_t wl_config_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, workload_t, wl);
	int ret;

	wl_notify(wl, WLS_CONFIGURING, 0, "%s", "");

	ret = wl->wl_type->wlt_wl_config(wl);

	if(ret != 0) {
		logmsg(LOG_WARN, "Failed to configure workload %s", wl->wl_name);

		THREAD_EXIT(ret);
	}

	if(wl->wl_tp) {
		tp_attach(wl->wl_tp, wl);
	}

	wl_notify(wl, WLS_CONFIGURED, 100, "%s", "");


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
 * Finish workload if it was stopped earlier
 */
void wl_try_finish_stopped(workload_t* wl) {
    boolean_t stopped = B_FALSE;
    
    mutex_lock(&wl->wl_status_mutex);
    stopped = WL_HAD_STATUS(wl, WLS_STOPPED);
    mutex_unlock(&wl->wl_status_mutex);
    
    if(!stopped)
        return;
    
    /* No more steps for stopped workloads */
    mutex_lock(&wl->wl_step_mutex);
    wl->wl_last_step = wl->wl_current_step;
    mutex_unlock(&wl->wl_step_mutex);
    
    wl_finish(wl);
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

	logmsg(LOG_TRACE, "Created request %s/%d step: %ld sched_time: %"PRItm, wl->wl_name,
					rq->rq_id, rq->rq_step, rq->rq_sched_time);

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

	logmsg(LOG_TRACE, "Cloned request %s/%d step: %ld sched_time: %"PRItm, wl->wl_name,
					rq->rq_id, rq->rq_step, rq->rq_sched_time);

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

	logmsg(LOG_TRACE, "Created trace-based request %s/%d step: %ld sched_time: %"PRItm, wl->wl_name,
			rq->rq_id, rq->rq_step, rq->rq_sched_time);

	return rq;
}

/**
 * Destroy request memory */
void wl_request_destroy(request_t* rq) {
	logmsg(LOG_TRACE, "Destroyed request %s/%d step: %ld thread: %d", rq->rq_workload->wl_name,
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

	logmsg(LOG_TRACE, "Running request %s/%d step: %ld worker: #%d sched: %"PRItm" start: %"PRItm,
		rq->rq_workload->wl_name, rq->rq_id, rq->rq_step, rq->rq_thread_id,
		rq->rq_sched_time, rq->rq_start_time);

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

tsobj_node_t* tsobj_request_format_all(list_head_t* rq_list) {
	tsobj_node_t* jrq;
	tsobj_node_t* j_rq_list = tsobj_new_array();

	request_t* rq;

	list_for_each_entry(request_t, rq, rq_list, rq_node) {
		jrq = tsobj_new_node("tsload.Request");

		tsobj_add_string(jrq, TSOBJ_STR("workload_name"),
						 tsobj_str_create(rq->rq_workload->wl_name));

		tsobj_add_integer(jrq, TSOBJ_STR("step"), rq->rq_step);
		tsobj_add_integer(jrq, TSOBJ_STR("request"), rq->rq_id);
		tsobj_add_integer(jrq, TSOBJ_STR("thread"), rq->rq_thread_id);

		tsobj_add_integer(jrq, TSOBJ_STR("sched"), rq->rq_sched_time);
		tsobj_add_integer(jrq, TSOBJ_STR("start"), rq->rq_start_time);
		tsobj_add_integer(jrq, TSOBJ_STR("end"), rq->rq_end_time);

		tsobj_add_integer(jrq, TSOBJ_STR("flags"), rq->rq_flags);

		tsobj_add_node(j_rq_list, NULL, jrq);
	}

	return j_rq_list;
}

static int tsobj_workload_proc_chain(workload_t* wl, tsobj_node_t* wl_chain_params) {
	workload_t* parent = NULL;

	const char* wl_chain_name = NULL;
	double wl_chain_probability = -1.0;
	randgen_t* wl_chain_rg = NULL;

	tsobj_node_t* probability;
	tsobj_node_t* randgen;
	int err;

	if(tsobj_check_type(wl_chain_params, JSON_NODE) != TSOBJ_OK)
		goto bad_tsobj;

	if(tsobj_get_string(wl_chain_params, "workload", &wl_chain_name) != TSOBJ_OK)
		goto bad_tsobj;

	err = tsobj_get_node(wl_chain_params, "probability", &probability);
	if(err == TSOBJ_INVALID_TYPE)
		goto bad_tsobj;

	if(tsobj_check_unused(wl_chain_params) != TSOBJ_OK)
		goto bad_tsobj;

	parent = wl_search(wl_chain_name);
	if(parent == NULL) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 WL_CHAIN_ERROR_PREFIX "not found workload '%s'",
						 wl->wl_name, wl_chain_name);
		goto fail;
	}

	if(err == TSOBJ_OK) {
		if(tsobj_get_node(probability, "randgen", &randgen) != TSOBJ_OK)
			goto bad_tsobj;

		if(tsobj_get_double(probability, "value", &wl_chain_probability) != TSOBJ_OK)
			goto bad_tsobj;

		if(tsobj_check_unused(probability) != TSOBJ_OK)
			goto bad_tsobj;

		if(wl_chain_probability < 0.0 || wl_chain_probability > 1.0) {
			tsload_error_msg(TSE_INVALID_VALUE,
							 WL_CHAIN_ERROR_PREFIX "invalid value for probability",
							 wl->wl_name);
			goto fail;
		}

		wl_chain_rg = tsobj_randgen_proc(randgen);

		if(wl_chain_rg == NULL) {
			tsload_error_msg(TSE_INVALID_VALUE,
							 WL_CHAIN_ERROR_PREFIX "failed to create random generator",
							 wl->wl_name);
			goto fail;
		}
	}

	wl_chain_back(parent, wl);

	wl->wl_chain_probability = wl_chain_probability;
	wl->wl_chain_rg = wl_chain_rg;

	return 0;

bad_tsobj:
	tsload_error_msg(tsobj_error_code(), WL_CHAIN_ERROR_PREFIX "%s",
					 wl->wl_name, tsobj_error_message());
fail:
	return -1;
}

workload_t* tsobj_workload_proc(const char* wl_name, const char* wl_type, const char* tp_name, ts_time_t deadline,
		                       tsobj_node_t* wl_chain_params, tsobj_node_t* rqsched_params, tsobj_node_t* wl_params) {
	workload_t* wl = NULL;
	wl_type_t* wlt = NULL;
	thread_pool_t* tp = NULL;

	int ret = RQSCHED_TSOBJ_OK;

	wlt = wl_type_search(wl_type);

	if(tp_name) {
		tp = tp_search(tp_name);
	}
	else if(!wl_chain_params) {
		tsload_error_msg(TSE_INVALID_VALUE,
						  WL_ERROR_PREFIX "neither 'chain' nor 'threadpool' was defined",
						  wl_name);
		goto fail;
	}

	logmsg(LOG_DEBUG, "Creating workload %s", wl_name);

	/* Create workload */
	wl = wl_create(wl_name, wlt, tp);

	/* Process request scheduler.
	 * Chained workloads do not have scheduler - use simple scheduler */
	if(wl_chain_params == NULL) {
		ret = tsobj_rqsched_proc(rqsched_params, wl);
	}
	else {
		wl->wl_rqsched_class = rqsched_find("simple");
		wl->wl_rqsched_private = rqsched_create(wl->wl_rqsched_class);
	}

	if(ret != RQSCHED_TSOBJ_OK) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 WL_ERROR_PREFIX "failed to create request scheduler",
						 wl_name, tsobj_error_message());

		goto fail;
	}

	/* Process params from i_params to wl_params, where mod_params contains
	 * parameters descriptions (see wlparam) */
	ret = tsobj_wlparam_proc_all(wl_params, wlt->wlt_params, wl);

	if(ret != WLPARAM_TSOBJ_OK) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 WL_ERROR_PREFIX "failed to process parameters",
						 wl_name, tsobj_error_message());


		goto fail;
	}

	/* Chain workload if needed */
	if(wl_chain_params != NULL) {
		ret = tsobj_workload_proc_chain(wl, wl_chain_params);

		if(ret != 0)
			goto fail;
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

