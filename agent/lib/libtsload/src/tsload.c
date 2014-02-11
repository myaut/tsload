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

#include <libjson.h>

#include <stdlib.h>

extern hashmap_t wl_type_hash_map;
extern hashmap_t tp_hash_map;

tsload_error_msg_func tsload_error_msg = NULL;
tsload_workload_status_func tsload_workload_status = NULL;
tsload_requests_report_func tsload_requests_report = NULL;

extern ts_time_t tp_min_quantum;
extern ts_time_t tp_max_quantum;

struct subsystem subsys[] = {
	SUBSYSTEM("log", log_init, log_fini),
	SUBSYSTEM("mempool", mempool_init, mempool_fini),
	SUBSYSTEM("threads", threads_init, threads_fini),
	SUBSYSTEM("wl_type", wlt_init, wlt_fini),
	SUBSYSTEM("modules", mod_init, mod_fini),
	SUBSYSTEM("workload", wl_init, wl_fini),
	SUBSYSTEM("threadpool", tp_init, tp_fini),
	SUBSYSTEM("netsock", nsk_init, nsk_fini),
};

static void* tsload_walkie_talkie(tsload_walk_op_t op, void* arg, hm_walker_func walker,
								  hashmap_t* hm, hm_json_formatter formatter) {
	switch(op) {
	case TSLOAD_WALK_FIND:
		return hash_map_find(hm, arg);
	case TSLOAD_WALK_JSON:
		return json_hm_format_bykey(hm, formatter, arg);
	case TSLOAD_WALK_JSON_ALL:
		return json_hm_format_all(hm, formatter);
	case TSLOAD_WALK_WALK:
		return hash_map_walk(hm, walker, arg);
	}

	return NULL;
}

void* tsload_walk_workload_types(tsload_walk_op_t op, void* arg, hm_walker_func walker) {
	return tsload_walkie_talkie(op, arg, walker, &wl_type_hash_map, json_wl_type_format);
}

JSONNODE* tsload_get_resources(void) {
	return json_hi_format_all(B_TRUE);
}

JSONNODE* tsload_get_hostinfo(void) {
	JSONNODE* node = json_new(JSON_NODE);

	json_push_back(node, json_new_a("hostname", hi_get_nodename()));

	json_push_back(node, json_new_a("domainname", hi_get_domainname()));
	json_push_back(node, json_new_a("osname", hi_get_os_name()));
	json_push_back(node, json_new_a("release", hi_get_os_release()));
	json_push_back(node, json_new_a("machine_arch", hi_get_mach()));

	json_push_back(node, json_new_i("num_cpus", hi_cpu_num_cpus()));
	json_push_back(node, json_new_i("num_cores", hi_cpu_num_cores()));
	json_push_back(node, json_new_i("mem_total", hi_cpu_mem_total()));

	json_push_back(node, json_new_i("agent_pid", t_get_pid()));

	return node;
}

int tsload_configure_workload(const char* wl_name, const char* wl_type, const char* tp_name, ts_time_t deadline,
							  JSONNODE* wl_chain_params, JSONNODE* rqsched_params, JSONNODE* wl_params) {
	workload_t* wl;

	if(wl_search(wl_name) != NULL) {
		tsload_error_msg(TSE_INVALID_DATA, "Workload '%s' already exists", wl_name);
		return TSLOAD_ERROR;
	}

	wl = json_workload_proc(wl_name, wl_type, tp_name, deadline, wl_chain_params, rqsched_params, wl_params);

	if(wl == NULL) {
		tsload_error_msg(TSE_INTERNAL_ERROR, "Error in tsload_configure_workload!");
		return TSLOAD_ERROR;
	}

	wl_config(wl);

	return TSLOAD_OK;
}

int tsload_provide_step(const char* wl_name, long step_id, unsigned num_rqs, list_head_t* trace_rqs, int* pstatus) {
	workload_t* wl = wl_search(wl_name);
	int ret;

	if(wl == NULL) {
		tsload_error_msg(TSE_INVALID_DATA, "Not found workload %s", wl_name);
		return TSLOAD_ERROR;
	}

	ret = wl_provide_step(wl, step_id, num_rqs, trace_rqs);

	switch(ret) {
	case WL_STEP_INVALID:
		tsload_error_msg(TSE_INVALID_DATA, "Step %ld is not correct, last step of workload '%s' was %ld!",
						 step_id, wl_name, wl->wl_last_step);
		return TSLOAD_ERROR;
	case WL_STEP_QUEUE_FULL:
	case WL_STEP_OK:
		*pstatus = ret;
		return TSLOAD_OK;
	}

	/* Shouldn't be here */
	tsload_error_msg(TSE_INTERNAL_ERROR, "Unknown result of wl_provide_step() call");
	return TSLOAD_ERROR;
}

/**
 * Create request and link it into rq_list
 *
 * If chained is B_TRUE, then it actually chains to last request in rq_list
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
		tsload_error_msg(TSE_INVALID_DATA, "Not found workload '%s'", wl_name);
		return TSLOAD_ERROR;
	}

	if(chained && list_empty(rq_list)) {
		tsload_error_msg(TSE_INVALID_DATA,
						 "Couldn't chain request to empty rq_list for workload '%s'", wl_name);
		return TSLOAD_ERROR;
	}

	rq = wl_create_request_trace(wl, rq_id, step, user_id, thread_id, sched_time, rq_params);

	if(rq == NULL) {
		/* Utilize previously created requests */
		tsload_error_msg(TSE_INTERNAL_ERROR,
						 "Failed to create trace based-request for workload '%s'", wl_name);
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

int tsload_start_workload(const char* wl_name, ts_time_t start_time) {
	workload_t* wl = wl_search(wl_name);

	if(wl == NULL) {
		tsload_error_msg(TSE_INVALID_DATA, "Not found workload '%s'", wl_name);
		return TSLOAD_ERROR;
	}

	if(wl->wl_status != WLS_CONFIGURED) {
		tsload_error_msg(TSE_INVALID_STATE, "Workload '%s' not in configured status", wl_name);
		return TSLOAD_ERROR;
	}

	wl->wl_start_time = start_time;
	return TSLOAD_OK;
}

int tsload_unconfigure_workload(const char* wl_name) {
	workload_t* wl = wl_search(wl_name);

	if(wl == NULL) {
		tsload_error_msg(TSE_INVALID_DATA, "Not found workload %s", wl_name);
		return TSLOAD_ERROR;
	}

	if(wl->wl_status == WLS_CONFIGURING) {
		tsload_error_msg(TSE_INVALID_STATE, "Workload %s is currently configuring", wl_name);
		return TSLOAD_ERROR;
	}

	wl_unconfig(wl);
	wl_destroy(wl);
	return TSLOAD_OK;
}

int tsload_create_threadpool(const char* tp_name, unsigned num_threads, ts_time_t quantum,
							 boolean_t discard, JSONNODE* disp) {
	thread_pool_t* tp = NULL;
	tp_disp_t* tpd = NULL;

	if(num_threads > TPMAXTHREADS || num_threads == 0) {
		tsload_error_msg(TSE_INVALID_DATA, "Too much or zero threads requested for tp '%s' (%u, max: %u)", tp_name, num_threads,
							TPMAXTHREADS);
		return TSLOAD_ERROR;
	}

	if(quantum < tp_min_quantum || quantum > tp_max_quantum) {
		tsload_error_msg(TSE_INVALID_DATA, "Invalid value of quantum requested for tp '%s' (%lld, min: %lld max: %lld)",
							tp_name, quantum, (long long) tp_min_quantum, (long long) tp_max_quantum);
		return TSLOAD_ERROR;
	}

	if(disp == NULL) {
		tsload_error_msg(TSE_INVALID_DATA, "Dispatcher properties are voluntary for tp '%s'", tp_name);
		return TSLOAD_ERROR;
	}

	tp = tp_search(tp_name);

	if(tp != NULL) {
		tsload_error_msg(TSE_INVALID_DATA, "Thread pool '%s' already exists", tp_name);
		return TSLOAD_ERROR;
	}

	tpd = json_tp_disp_proc(disp);

	if(tpd == NULL) {
		return TSLOAD_ERROR;
	}

	tp = tp_create(tp_name, num_threads, quantum, discard, tpd);

	if(tp == NULL) {
		tsload_error_msg(TSE_INTERNAL_ERROR, "Internal error occured while creating threadpool");
		return TSLOAD_ERROR;
	}

	return TSLOAD_OK;
}

int tsload_schedule_threadpool(const char* tp_name, JSONNODE* sched) {
	thread_pool_t* tp = tp_search(tp_name);
	int ret;

	if(tp == NULL) {
		tsload_error_msg(TSE_INVALID_DATA, "Thread pool '%s' not exists", tp_name);
		return TSLOAD_ERROR;
	}

	ret = json_tp_schedule(tp, sched);

	if(ret != 0) {
		tsload_error_msg(TSE_INTERNAL_ERROR, "Couldn't set scheduler options for threadpool '%s'", tp_name);
		return TSLOAD_ERROR;
	}

	return TSLOAD_OK;
}

void* tsload_walk_threadpools(tsload_walk_op_t op, void* arg, hm_walker_func walker) {
	return tsload_walkie_talkie(op, arg, walker, &tp_hash_map, json_tp_format);
}

int tsload_destroy_threadpool(const char* tp_name) {
	thread_pool_t* tp = tp_search(tp_name);

	if(tp == NULL) {
		tsload_error_msg(TSE_INVALID_DATA, "Thread pool '%s' not exists", tp_name);
		return TSLOAD_ERROR;
	}

	if(tp->tp_wl_count > 0) {
		tsload_error_msg(TSE_INVALID_STATE, "Thread pool '%s' has workloads running on it", tp_name);
		return TSLOAD_ERROR;
	}

	tp_destroy(tp);
	return TSLOAD_OK;
}

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

