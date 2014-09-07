/*
 * init.c
 *
 *  Created on: Dec 3, 2012
 *      Author: myaut
 */

#define LOG_SOURCE ""
#include <log.h>

#include <defs.h>
#include <threads.h>
#include <mempool.h>
#include <workload.h>
#include <modules.h>
#include <wltype.h>
#include <threadpool.h>
#include <tpdisp.h>
#include <tstime.h>
#include <netsock.h>

#include <uname.h>
#include <hiobject.h>
#include <cpuinfo.h>

#include <tsinit.h>
#include <tsload.h>

#include <tsobj.h>

#include <stdlib.h>

extern hashmap_t wl_type_hash_map;
extern hashmap_t tp_hash_map;

tsload_error_msg_func tsload_error_msg = NULL;
tsload_workload_status_func tsload_workload_status = NULL;
tsload_requests_report_func tsload_requests_report = NULL;

extern ts_time_t tp_min_quantum;
extern ts_time_t tp_max_quantum;
extern int tp_max_threads;

struct subsystem subsys[] = {
	SUBSYSTEM("log", log_init, log_fini),
	SUBSYSTEM("mempool", mempool_init, mempool_fini),
	SUBSYSTEM("threads", threads_init, threads_fini),
	SUBSYSTEM("wl_type", wlt_init, wlt_fini),
	SUBSYSTEM("modules", mod_init, mod_fini),
	SUBSYSTEM("workload", wl_init, wl_fini),
	SUBSYSTEM("threadpool", tp_init, tp_fini),
	SUBSYSTEM("netsock", nsk_init, nsk_fini),
	SUBSYSTEM("json", json_init, json_fini),
	SUBSYSTEM("tsobj", tsobj_init, tsobj_fini),
};

static void* tsload_walkie_talkie(tsload_walk_op_t op, void* arg, hm_walker_func walker,
								  hashmap_t* hm, hm_tsobj_formatter formatter,
								  hm_tsobj_key_formatter key_formatter) {
	switch(op) {
	case TSLOAD_WALK_FIND:
		return hash_map_find(hm, arg);
	case TSLOAD_WALK_TSOBJ:
		return tsobj_hm_format_bykey(hm, formatter, arg);
	case TSLOAD_WALK_TSOBJ_ALL:
		return tsobj_hm_format_all(hm, formatter, key_formatter);
	case TSLOAD_WALK_WALK:
		return hash_map_walk(hm, walker, arg);
	}

	return NULL;
}

/**
 * Walk over workload types registered within TSLoad
 */
void* tsload_walk_workload_types(tsload_walk_op_t op, void* arg, hm_walker_func walker) {
	return tsload_walkie_talkie(op, arg, walker, &wl_type_hash_map, tsobj_wl_type_format, NULL);
}

tsobj_node_t* tsload_get_resources(void) {
	return tsobj_hi_format_all(B_TRUE);
}

tsobj_node_t* tsload_get_hostinfo(void) {
	tsobj_node_t* node = tsobj_new_node("tsload.HostInfo");

	tsobj_add_string(node, TSOBJ_STR("hostname"), tsobj_str_create(hi_get_nodename()));

	tsobj_add_string(node, TSOBJ_STR("domainname"), tsobj_str_create(hi_get_domainname()));
	tsobj_add_string(node, TSOBJ_STR("osname"), tsobj_str_create(hi_get_os_name()));
	tsobj_add_string(node, TSOBJ_STR("release"), tsobj_str_create(hi_get_os_release()));
	tsobj_add_string(node, TSOBJ_STR("machine_arch"), tsobj_str_create(hi_get_mach()));
	tsobj_add_string(node, TSOBJ_STR("system"), tsobj_str_create(hi_get_sys_name()));

	tsobj_add_integer(node, TSOBJ_STR("num_cpus"), hi_cpu_num_cpus());
	tsobj_add_integer(node, TSOBJ_STR("num_cores"), hi_cpu_num_cores());
	tsobj_add_integer(node, TSOBJ_STR("mem_total"), hi_cpu_mem_total());

	tsobj_add_integer(node, TSOBJ_STR("agent_pid"), t_get_pid());

	return node;
}

/**
 * Create and configure new workload
 *
 * @param wl_name name of workload
 * @param wl_type name of workload type
 * @param tp_name name of threadpool where workload would be attached. For chained workloads \
 * 			should be NULL
 * @param deadline deadline for request execution
 * @param wl_chain_params parameters of chaining in [experiment.json][ref/experiment_json] format
 * @param rqsched_params parameters of request scheduler in [experiment.json][ref/experiment_json] format
 * @param wl_params workload and request param configuration
 */
int tsload_configure_workload(const char* wl_name, const char* wl_type, const char* tp_name, ts_time_t deadline,
							  tsobj_node_t* wl_chain_params, tsobj_node_t* rqsched_params, tsobj_node_t* wl_params) {
	workload_t* wl;

	if(wl_search(wl_name) != NULL) {
		tsload_error_msg(TSE_ALREADY_EXISTS,
						 TSLOAD_CONFIGURE_WORKLOAD_ERROR_PREFIX "already exists", wl_name);
		return TSLOAD_ERROR;
	}

	wl = tsobj_workload_proc(wl_name, wl_type, tp_name, deadline, wl_chain_params, rqsched_params, wl_params);

	if(wl == NULL) {
		return TSLOAD_ERROR;
	}

	wl_config(wl);

	return TSLOAD_OK;
}

/**
 * Provide step for workload
 *
 * @param wl_name name of workload
 * @param step_id id of step. This argument is used to control that no tsload_provide_step calls was missed
 * @param num_rqs number of request that should be generated
 * @param trace_rqs queue of requests that created earlier from trace through tsload_create_request(). \
 * 	 If this list not empty, than TSLoad would generate num_rqs requests
 * @param pstatus output field that set to WL_STEP_QUEUE_FULL if function returns TSLOAD_OK, but queue of \
 * 	 workload requests is overrun.
 */
int tsload_provide_step(const char* wl_name, long step_id, unsigned num_rqs, list_head_t* trace_rqs, int* pstatus) {
	workload_t* wl = wl_search(wl_name);
	int ret;

	if(wl == NULL) {
		tsload_error_msg(TSE_NOT_FOUND,
						 TSLOAD_PROVIDE_STEP_ERROR_PREFIX "workload not found", wl_name);
		return TSLOAD_ERROR;
	}

	ret = wl_provide_step(wl, step_id, num_rqs, trace_rqs);

	switch(ret) {
	case WL_STEP_INVALID:
		tsload_error_msg(TSE_INVALID_STATE,
						 TSLOAD_PROVIDE_STEP_ERROR_PREFIX "step %ld is not correct, last step was %ld!",
						 wl_name, step_id, wl->wl_last_step);
		return TSLOAD_ERROR;
	case WL_STEP_QUEUE_FULL:
	case WL_STEP_OK:
		*pstatus = ret;
		return TSLOAD_OK;
	}

	/* Shouldn't be here */
	tsload_error_msg(TSE_INTERNAL_ERROR,
					 TSLOAD_PROVIDE_STEP_ERROR_PREFIX "unknown result %d of wl_provide_step() call",
					 wl_name, ret);
	return TSLOAD_ERROR;
}

/**
 * Create request and link it into rq_list to create trace-driven workloads
 *
 * If chained is set B_TRUE, then it actually chains to last request in rq_list
 *
 * @param wl_name name of workload
 * @param rq_list linked list where new request should be put
 * @param chained is request chained?
 * @param rq_id id of request
 * @param step id of step. Not neccessary be current/next step of workload
 * @param user_id Id of user for user threadpool dispatcher
 * @param thread_id Id of thread for trace threadpool dispatcher
 * @param sched_time Scheduled time of request execution
 * @param rq_params Vector of request params (copied inside this function, so \
 * 		it could be safely deallocated after call)
 *
 * @note If error occurs, tsload_create_request() will empty rq_list
 */
int tsload_create_request(const char* wl_name, list_head_t* rq_list, boolean_t chained,
		 	 	 	 	   int rq_id, long step, int user_id, int thread_id,
						   ts_time_t sched_time, void* rq_params) {
	workload_t* wl = wl_search(wl_name);
	request_t* rq;
	request_t* rq_parent;

	if(wl == NULL) {
		tsload_error_msg(TSE_NOT_FOUND,
						 TSLOAD_CREATE_REQUEST_ERROR_PREFIX "workload not found", wl_name);
		return TSLOAD_ERROR;
	}

	if(chained && list_empty(rq_list)) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 TSLOAD_CREATE_REQUEST_ERROR_PREFIX "empty rq_list", wl_name);
		return TSLOAD_ERROR;
	}

	rq = wl_create_request_trace(wl, rq_id, step, user_id, thread_id, sched_time, rq_params);

	if(rq == NULL) {
		tsload_error_msg(TSE_INTERNAL_ERROR,
						 TSLOAD_CREATE_REQUEST_ERROR_PREFIX "internal error", wl_name);
		/* Utilize previously created requests */
		wl_destroy_request_list(rq_list);
		return TSLOAD_ERROR;
	}

	if(chained) {
		rq_parent = list_last_entry(request_t, rq_list, rq_node);
		while(rq_parent->rq_chain_next != NULL) {
			rq_parent = rq_parent->rq_chain_next;
		}
		rq_parent->rq_chain_next = rq;
	}
	else {
		list_add_tail(&rq->rq_node, rq_list);
	}

	return TSLOAD_OK;
}

/**
 * Schedule workload to start
 *
 * @param wl_name name of workload
 * @param start_time scheduled start time
 */
int tsload_start_workload(const char* wl_name, ts_time_t start_time) {
	workload_t* wl = wl_search(wl_name);

	if(wl == NULL) {
		tsload_error_msg(TSE_NOT_FOUND,
				         TSLOAD_START_WORKLOAD_ERROR_PREFIX "workload not found", wl_name);
		return TSLOAD_ERROR;
	}

	if(wl->wl_status != WLS_CONFIGURED) {
		tsload_error_msg(TSE_INVALID_STATE,
						 TSLOAD_START_WORKLOAD_ERROR_PREFIX "not yet configured", wl_name);
		return TSLOAD_ERROR;
	}

	wl->wl_start_time = start_time;
	return TSLOAD_OK;
}

/**
 * Unconfigure workload. Couldn't be called for workload that
 * currenly have "WLS_CONFIGURING" state because we couldn't interrupt
 * module routine
 *
 * @param wl_name name of workload
 */
int tsload_unconfigure_workload(const char* wl_name) {
	workload_t* wl = wl_search(wl_name);

	if(wl == NULL) {
		tsload_error_msg(TSE_NOT_FOUND,
						 TSLOAD_UNCONFIGURE_WORKLOAD_ERROR_PREFIX "workload not found", wl_name);
		return TSLOAD_ERROR;
	}

	if(wl->wl_status == WLS_CONFIGURING) {
		tsload_error_msg(TSE_INVALID_STATE,
						 TSLOAD_UNCONFIGURE_WORKLOAD_ERROR_PREFIX "workload currently configuring", wl_name);
		return TSLOAD_ERROR;
	}

	wl_unconfig(wl);
	wl_destroy(wl);
	return TSLOAD_OK;
}

/**
 * Create threadpool
 *
 * @param tp_name name of threadpool
 * @param num_threads number of threads inside threadpool
 * @param quantum threadpool's quantum
 * @param discard if set to B_TRUE, threadpool will discard requests that missed their step
 * @param disp parameters of threadpool dispatcher according to [experiment.json][ref/experiment_json] format
 */
int tsload_create_threadpool(const char* tp_name, unsigned num_threads, ts_time_t quantum,
							 boolean_t discard, tsobj_node_t* disp) {
	thread_pool_t* tp = NULL;
	tp_disp_t* tpd = NULL;

	if(num_threads > tp_max_threads || num_threads == 0) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 TSLOAD_CREATE_THREADPOOL_ERROR_PREFIX "too much or zero threads (%u, max: %u)", tp_name, num_threads,
						 TPMAXTHREADS);
		return TSLOAD_ERROR;
	}

	if(quantum < tp_min_quantum || quantum > tp_max_quantum) {
		tsload_error_msg(TSE_INVALID_VALUE,
				 	 	 TSLOAD_CREATE_THREADPOOL_ERROR_PREFIX "invalid value of quantum (%lld, min: %lld max: %lld)",
						 tp_name, quantum, (long long) tp_min_quantum, (long long) tp_max_quantum);
		return TSLOAD_ERROR;
	}

	if(disp == NULL) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 TSLOAD_CREATE_THREADPOOL_ERROR_PREFIX "dispatcher properties are required",
						 tp_name);
		return TSLOAD_ERROR;
	}

	tp = tp_search(tp_name);

	if(tp != NULL) {
		tsload_error_msg(TSE_ALREADY_EXISTS,
						 TSLOAD_CREATE_THREADPOOL_ERROR_PREFIX "already exists",
						 tp_name);
		return TSLOAD_ERROR;
	}

	tpd = tsobj_tp_disp_proc(disp);

	if(tpd == NULL) {
		return TSLOAD_ERROR;
	}

	tp = tp_create(tp_name, num_threads, quantum, discard, tpd);

	if(tp == NULL) {
		tsload_error_msg(TSE_INTERNAL_ERROR,
						 TSLOAD_CREATE_THREADPOOL_ERROR_PREFIX "internal error", tp_name);
		return TSLOAD_ERROR;
	}

	return TSLOAD_OK;
}

/**
 * Set threadpool scheduler options
 *
 * @param tp_name name of threadpool
 * @param sched scheduling parameters according to [experiment.json][ref/experiment_json] format
 */
int tsload_schedule_threadpool(const char* tp_name, tsobj_node_t* sched) {
	thread_pool_t* tp = tp_search(tp_name);
	int ret;

	if(tp == NULL) {
		tsload_error_msg(TSE_NOT_FOUND,
						 TSLOAD_SCHED_THREADPOOL_ERROR_PREFIX "not found", tp_name);
		return TSLOAD_ERROR;
	}

	ret = tsobj_tp_schedule(tp, sched);

	if(ret != 0) {
		return TSLOAD_ERROR;
	}

	return TSLOAD_OK;
}

void* tsload_walk_threadpools(tsload_walk_op_t op, void* arg, hm_walker_func walker) {
	return tsload_walkie_talkie(op, arg, walker, &tp_hash_map, tsobj_tp_format, NULL);
}

int tsload_destroy_threadpool(const char* tp_name) {
	thread_pool_t* tp = tp_search(tp_name);

	if(tp == NULL) {
		tsload_error_msg(TSE_NOT_FOUND,
						 TSLOAD_SCHED_THREADPOOL_ERROR_PREFIX "not found", tp_name);
		return TSLOAD_ERROR;
	}

	if(tp->tp_wl_count > 0) {
		tsload_error_msg(TSE_INVALID_STATE,
						 TSLOAD_DESTROY_THREADPOOL_ERROR_PREFIX "there are workloads attached to it",
						 tp_name);
		return TSLOAD_ERROR;
	}

	tp_destroy(tp);
	return TSLOAD_OK;
}

/**
 * Initialize TSLoad engine
 *
 * @param pre_subsys array of subsystems that should be initialized before libtsload subsystems
 * @param pre_count count of elements in pre_subsys
 * @param post_subsys array of subsystems that initialized after libtsload
 * @param post_count count of elements in post_subsys
 */
int tsload_init(struct subsystem* pre_subsys, unsigned pre_count,
				struct subsystem* post_subsys, unsigned post_count) {
	struct subsystem** subsys_list = NULL;
	unsigned s_count = sizeof(subsys) / sizeof(struct subsystem);
	int si = 0, xsi;
	unsigned count = s_count + pre_count + post_count;

	/* Mempool is not yet initialized, so use libc; freed by ts_finish */
	subsys_list = malloc(count * sizeof(struct subsystem*));

	/* Initialize [0:s_count] subsystems with tsload's subsystems
	 * and [s_count:s_count+xs_count] with extended subsystems */
	for(xsi = 0; xsi < pre_count; ++si, ++xsi)
		subsys_list[si] = &pre_subsys[xsi];
	for(xsi = 0; xsi < s_count; ++si, ++xsi)
		subsys_list[si] = &subsys[xsi];
	for(xsi = 0; xsi < post_count; ++si, ++xsi)
		subsys_list[si] = &post_subsys[xsi];

	return ts_init(subsys_list, count);
}

int tsload_start(const char* basename) {
	logmsg(LOG_INFO, "Started %s on %s...", basename, hi_get_nodename());
	logmsg(LOG_INFO, "Clock resolution is %llu", tm_get_clock_res());

	/*Wait until universe collapses or we receive a signal :)*/
	t_eternal_wait();

	/*NOTREACHED*/
	return 0;
}

