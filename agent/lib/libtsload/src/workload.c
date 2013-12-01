/*
 * workload.c
 *
 *  Created on: Nov 19, 2012
 *      Author: myaut
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
#include <disp.h>

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

/**
 * TSLoad supports statically-defined userspace dynamic tracing (USDT) using etrace subsystem.
 * Build with --enable-usdt (Linux + SystemTap or Solaris + DTrace) or --enable-etw
 * (Event Tracing for Windows).
 *
 * Windows:
 *		Register provider with wevtutil:
 *			C:\> wevtutil im Shared\workload.man
 *		Start tracing with logman:
 *		 	C:\> logman start -ets TSloadWL -p "Tslaod-Workload" 0 0 -o tracelog.etl
 *		Run run-tsload:
 *			C:\> run-tsload.bat -e Data\busy_wait\
 *		Stop tracing:
 *			C:\> logman stop -ets TSloadWL
 *		Generate reports:
 *			C:\> tracerpt -y tracelog.etl
 *
 *		If you need to uninstall provider, call wevtutil with um command:
 *			C:\> wevtutil um Shared\workload.man
 *
 * Linux:
 * 		root@centos# stap -e '
 * 			probe process("../lib/libtsload.so").provider("tsload__workload").mark("request__start") {
 * 				println(user_string(@cast($arg1, "workload_t")->wl_name));
 * 			} ' \
 * 				-c 'run-tsload -e ../var/busy_wait/'
 *
 * Solaris:
 * 		root@sol11# dtrace -n '
 * 			request-start {
 * 				this->wl_name = (char*) copyin(arg0, 64);
 * 				trace(stringof(this->wl_name));
 * 			}'	\
 * 				 -c 'run-tsload -e ../var/busy_wait/'
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

/**
 * wl_create - create new workload: allocate memory and initialize fields
 *
 * @return NULL if malloc had failed or new workload object
 */
workload_t* wl_create(const char* name, wl_type_t* wlt, thread_pool_t* tp) {
	workload_t* wl = (workload_t*) mp_cache_alloc(&wl_cache);

	if(wl == NULL)
		return NULL;

	strncpy(wl->wl_name, name, WLNAMELEN);
	wl->wl_type = wlt;

	wl->wl_tp = tp;

	wl->wl_current_rq = 0;
	wl->wl_current_step = -1;
	wl->wl_last_step = -1;

	wl->wl_hm_next = NULL;

	list_node_init(&wl->wl_chain);
	list_node_init(&wl->wl_tp_node);

	wl->wl_params = mp_malloc(wlt->wlt_params_size);
	wl->wl_private = NULL;

	wl->wl_is_configured = B_FALSE;
	wl->wl_is_started = B_FALSE;
	wl->wl_status = WLS_NEW;

	wl->wl_start_time = TS_TIME_MAX;

	wl->wl_notify_time = 0;

	wl->wl_disp_class = NULL;
	wl->wl_disp_private = NULL;

	mutex_init(&wl->wl_rq_mutex, "wl-%s-rq", name);

	hash_map_insert(&workload_hash_map, wl);

	return wl;
}

/**
 * wl_destroy_nodetach - workload destructor
 *
 * */
void wl_destroy_nodetach(workload_t* wl) {
	hash_map_remove(&workload_hash_map, wl);

	wl->wl_disp_class->disp_fini(wl);

	if(wl->wl_status != WLS_NEW)
		t_destroy(&wl->wl_cfg_thread);

	list_del(&wl->wl_chain);

	mutex_destroy(&wl->wl_rq_mutex);

	mp_free(wl->wl_params);
	mp_cache_free(&wl_cache, wl);
}

/**
 * wl_destroy - destroy workload and detach it from threadpool
 * */
void wl_destroy(workload_t* wl) {
	if(wl->wl_tp)
		tp_detach(wl->wl_tp, wl);

	wl_destroy_nodetach(wl);
}

/**
 * wl_destroy_all - free memory for chain of workload objects
 * */
void wl_destroy_all(list_head_t* wl_head) {
	workload_t *wl, *wl_tmp;

	list_for_each_entry_safe(workload_t, wl, wl_tmp, wl_head, wl_chain) {
		wl_destroy(wl);
	}
}

workload_t* wl_search(const char* name) {
	return hash_map_find(&workload_hash_map, name);
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

	/* Ignore intermediate progress messages if they are going too fasy */
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
	case WLS_FAIL:
		progress = -1;
	break;
	case WLS_SUCCESS:
		progress = 100;
	break;
	case WLS_RUNNING:
	case WLS_FINISHED:
		progress = wl->wl_current_step;
		break;
	default:
		assert(status == WLS_CONFIGURING);
	break;
	}

	wl->wl_status = status;

	msg = mp_malloc(sizeof(wl_notify_msg_t));

	va_start(args, format);
	vsnprintf(msg->msg, 512, format, args);
	va_end(args);

	logmsg((status == WLS_FAIL)? LOG_WARN : LOG_INFO,
			"Workload %s status: %s", wl->wl_name, msg->msg);

	msg->wl = wl;
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

		tsload_workload_status(msg->wl->wl_name, msg->status, msg->progress, msg->msg);

		mp_free(msg);
	}

THREAD_END:
	THREAD_FINISH(arg);
}

thread_result_t wl_config_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, workload_t, wl);
	int ret;

	logmsg(LOG_INFO, "Started configuring of workload %s", wl->wl_name);

	ret = wl->wl_type->wlt_wl_config(wl);

	if(ret != 0) {
		logmsg(LOG_WARN, "Failed to configure workload %s", wl->wl_name);

		THREAD_EXIT(ret);
	}

	tp_attach(wl->wl_tp, wl);
	wl->wl_is_configured = B_TRUE;

	wl_notify(wl, WLS_SUCCESS, 100, "Successfully configured workload %s", wl->wl_name);


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
	if(wl->wl_is_started)
		return B_TRUE;

	if(wl->wl_is_configured &&
	   tm_get_time() >= wl->wl_start_time) {
		logmsg(LOG_INFO, "Starting workload %s...", wl->wl_name);

		wl->wl_is_started = B_TRUE;
		return B_TRUE;
	}

	return B_FALSE;
}

/**
 * Provide number of requests for current step
 *
 * Responds to agent:
 * 	- Error AE_INVALID_DATA if step_id is incorrect
 * 	- Response { "success": true | false, "reason": "Reason" }
 *
 * @param wl workload
 * @param step_id step id to be provided
 * @param num_rqs number
 *
 * @return 0 if steps are saved, -1 if incorrect step provided or wl_requests is full
 * */
int wl_provide_step(workload_t* wl, long step_id, unsigned num_rqs) {
	int ret = WL_STEP_OK;
	long step_off = step_id & WLSTEPQMASK;

	mutex_lock(&wl->wl_rq_mutex);

	if((wl->wl_last_step - wl->wl_current_step) == WLSTEPQSIZE) {
		ret = WL_STEP_QUEUE_FULL;
		goto done;
	}

	if(step_id != (wl->wl_last_step + 1)) {
		ret = WL_STEP_INVALID;
		goto done;
	}

	wl->wl_last_step = step_id;
	wl->wl_rqs_per_step[step_off] = num_rqs;

done:
	mutex_unlock(&wl->wl_rq_mutex);

	return ret;
}

/**
 * Proceed to next step, and return number of requests in it
 *
 * @param wl processing workload
 * @param p_num_rqs variable where number of requests is saved. Cannot be NULL
 *
 * @return 0 if operation successful or -1 if not steps on queue
 * */
int wl_advance_step(workload_step_t* step) {
	int ret = 0;
	long step_off;

	workload_t* wl = NULL;

	assert(step != NULL);

	wl = step->wls_workload;

	mutex_lock(&wl->wl_rq_mutex);

	wl->wl_current_step++;
	wl->wl_current_rq = 0;

	if(wl->wl_current_step > wl->wl_last_step) {
		/* No steps on queue */
		logmsg(LOG_WARN, "No steps on queue %s, step #%ld!", wl->wl_name, wl->wl_current_step);

		ret = -1;
		goto done;
	}

	ETRC_PROBE1(tsload__workload, workload__step, workload_t*, wl);

	step_off = wl->wl_current_step  & WLSTEPQMASK;
	step->wls_rq_count = wl->wl_rqs_per_step[step_off];

	wl->wl_disp_class->disp_step(step);

done:
	mutex_unlock(&wl->wl_rq_mutex);

	return ret;
}

/**
 * Create request structure, append it to requests queue, initialize
 *
 * @param wl workload for request
 * @param thread_id id of thread that will execute thread
 * */
request_t* wl_create_request(workload_t* wl, int thread_id) {
	request_t* rq = (request_t*) mp_cache_alloc(&wl_rq_cache);

	rq->rq_step = wl->wl_current_step;
	rq->rq_id = wl->wl_current_rq++;

	rq->rq_thread_id = thread_id;

	rq->rq_flags = 0;

	rq->rq_workload = wl;
	strcpy(rq->rq_wl_name, wl->wl_name);

	rq->rq_sched_time = 0;
	rq->rq_start_time = 0;
	rq->rq_end_time = 0;

	wl->wl_disp_class->disp_pre_request(rq);

	list_node_init(&rq->rq_node);

	return rq;
}

/**
 * Free request's memory */
void wl_request_free(request_t* rq) {
	mp_cache_free(&wl_rq_cache, rq);
}

/**
 * Run request for execution */
void wl_run_request(request_t* rq) {
	int ret;
	workload_t* wl = rq->rq_workload;

	rq->rq_flags |= RQF_STARTED;

	ETRC_PROBE2(tsload__workload, request__start, workload_t*, wl, request_t*, rq);

	rq->rq_start_time = tm_get_clock();
	ret = wl->wl_type->wlt_run_request(rq);
	rq->rq_end_time = tm_get_clock();

	if(rq->rq_start_time <= rq->rq_sched_time)
		rq->rq_flags |= RQF_ONTIME;

	if(ret == 0)
		rq->rq_flags |= RQF_SUCCESS;

	rq->rq_flags |= RQF_FINISHED;

	wl->wl_disp_class->disp_post_request(rq);

	ETRC_PROBE2(tsload__workload, request__finish, workload_t*, wl, request_t*, rq);
}

void wl_rq_chain_push(list_head_t* rq_chain) {
	squeue_push(&wl_requests, rq_chain);
}

void wl_rq_chain_destroy(void *p_rq_chain) {
	list_head_t* rq_chain = (list_head_t*) p_rq_chain;
	request_t *rq, *rq_tmp;

	list_for_each_entry_safe(request_t, rq, rq_tmp, rq_chain, rq_node) {
		wl_request_free(rq);
	}

	mp_free(p_rq_chain);
}

thread_result_t wl_requests_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, void, unused);
	list_head_t* rq_chain;

	JSONNODE* j_rq_chain;

	request_t *rq, *rq_tmp;

	while(B_TRUE) {
		rq_chain =  (list_head_t*) squeue_pop(&wl_requests);

		if(rq_chain == NULL) {
			THREAD_EXIT(0);
		}

		tsload_requests_report(rq_chain);

		wl_rq_chain_destroy(rq_chain);
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

		json_push_back(jrq, json_new_a("workload_name", rq->rq_wl_name));

		json_push_back(jrq, json_new_i("step", rq->rq_step));
		json_push_back(jrq, json_new_i("request", rq->rq_id));
		json_push_back(jrq, json_new_i("thread", rq->rq_thread_id));

		json_push_back(jrq, json_new_i("start", rq->rq_start_time));
		json_push_back(jrq, json_new_i("end", rq->rq_end_time));

		json_push_back(jrq, json_new_i("flags", rq->rq_flags));

		json_push_back(j_rq_list, jrq);
	}

	return j_rq_list;
}

#define JSON_GET_VALIDATE_PARAM(iter, name, req_type)	\
	{											\
		iter = json_find(node, name);			\
		if(iter == i_end) {						\
			tsload_error_msg(TSE_MESSAGE_FORMAT,	\
					"Failed to parse workload, missing parameter %s", name);	\
			goto fail;							\
		}										\
		if(json_type(*iter) != req_type) {		\
			tsload_error_msg(TSE_MESSAGE_FORMAT,	\
					"Expected that " name " is " #req_type );	\
			goto fail;							\
		}										\
	}

#define SEARCH_OBJ(dest, iter, type, search) 		\
	do {									\
		type* obj = NULL;					\
		char* name = json_as_string(*iter);	\
		obj = search(name);					\
		json_free(name);					\
		if(obj == NULL) {					\
			tsload_error_msg(TSE_INVALID_DATA,				\
					"Invalid " #type " name %s", name);		\
			goto fail;						\
		}									\
		dest = obj;							\
	} while(0);


/**
 * Process incoming workload in format
 * {
 * 		"module": "..."
 * 		"threadpool": "...",
 * 		"params": {}
 * }
 * and create workload. Called from agent context,
 * so returns NULL in case of error and invokes agent_error_msg
 * */
workload_t* json_workload_proc(const char* wl_name, JSONNODE* node) {
	JSONNODE_ITERATOR i_wlt = NULL;
	JSONNODE_ITERATOR i_tp = NULL;
	JSONNODE_ITERATOR i_params = NULL;
	JSONNODE_ITERATOR i_end = json_end(node);

	workload_t* wl = NULL;
	wl_type_t* wlt = NULL;
	thread_pool_t* tp = NULL;

	int ret;

	/* Get threadpool and module from incoming message
	 * and find corresponding objects*/
	JSON_GET_VALIDATE_PARAM(i_wlt, "wltype", JSON_STRING);
	JSON_GET_VALIDATE_PARAM(i_tp, "threadpool", JSON_STRING);
	JSON_GET_VALIDATE_PARAM(i_params, "params", JSON_NODE);

	SEARCH_OBJ(wlt, i_wlt, wl_type_t, wl_type_search);
	SEARCH_OBJ(tp, i_tp, thread_pool_t, tp_search);

	/* Get workload's name */
	logmsg(LOG_DEBUG, "Parsing workload %s", wl_name);

	if(strlen(wl_name) == 0) {
		tsload_error_msg(TSE_MESSAGE_FORMAT,"Failed to parse workload, no name was defined");
		goto fail;
	}

	if(wl_search(wl_name) != NULL) {
		tsload_error_msg(TSE_INVALID_DATA, "Workload %s already exists", wl_name);
		goto fail;
	}

	/* Create workload */
	wl = wl_create(wl_name, wlt, tp);

	/* Process request dispatcher */
	ret = json_disp_proc(node, wl);

	if(ret != DISP_JSON_OK) {
		goto fail;
	}

	/* Process params from i_params to wl_params, where mod_params contains
	 * parameters descriptions (see wlparam) */
	ret = json_wlparam_proc_all(*i_params, wlt->wlt_params, wl->wl_params);

	if(ret != WLPARAM_JSON_OK) {
		goto fail;
	}

	return wl;

fail:
	if(wl)
		wl_destroy(wl);

	return NULL;
}

int wl_init(void) {
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

	squeue_push(&wl_requests, NULL);
	t_destroy(&t_wl_requests);
	squeue_destroy(&wl_requests, wl_rq_chain_destroy);

	squeue_push(&wl_notifications, NULL);
	t_destroy(&t_wl_notify);
	squeue_destroy(&wl_notifications, mp_free);

	hash_map_destroy(&workload_hash_map);

	mp_cache_destroy(&wl_rq_cache);
	mp_cache_destroy(&wl_cache);
}
