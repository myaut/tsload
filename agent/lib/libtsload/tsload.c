
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



#define LOG_SOURCE ""
#include <tsload/log.h>

#include <tsload/defs.h>

#include <tsload/mempool.h>
#include <tsload/time.h>
#include <tsload/init.h>
#include <tsload/netsock.h>
#include <tsload/execpath.h>

#include <tsload/obj/obj.h>

#include <hostinfo/uname.h>
#include <hostinfo/hiobject.h>
#include <hostinfo/cpuinfo.h>

#include <tsload/load/workload.h>
#include <tsload/load/wltype.h>
#include <tsload/load/threadpool.h>
#include <tsload/load/tpdisp.h>
#include <tsload/load/randgen.h>
#include <tsload/load/rqsched.h>
#include <tsload.h>

#include <errormsg.h>

#include <stdlib.h>

extern hashmap_t randgen_hash_map;
extern hashmap_t randvar_hash_map;
extern hashmap_t tpd_hash_map;
extern hashmap_t wl_type_hash_map;
extern hashmap_t tp_hash_map;
extern hashmap_t rqsvar_hash_map;
extern hashmap_t rqsched_hash_map;

tsload_error_msg_func tsload_error_msg = NULL;
tsload_workload_status_func tsload_workload_status = NULL;
tsload_requests_report_func tsload_requests_report = NULL;

extern ts_time_t tp_min_quantum;
extern ts_time_t tp_max_quantum;
extern int tp_max_threads;

void tsload_register_error_msg_func(tsload_error_msg_func func) {
	tsload_error_msg = func;
}

void tsload_register_workload_status_func(tsload_workload_status_func func) {
	tsload_workload_status = func;
}

void tsload_register_requests_report_func(tsload_requests_report_func func) {
	tsload_requests_report = func;
}

void tsobj_module_format_helper(tsobj_node_t* parent, module_t* mod) {
	if(mod != NULL) {
		tsobj_add_string(parent, TSOBJ_STR("module"), 
						tsobj_str_create(mod->mod_name));
		tsobj_add_string(parent, TSOBJ_STR("path"), 
						tsobj_str_create(mod->mod_path));
	}
	else {
		tsobj_add_string(parent, TSOBJ_STR("module"), 
						 TSOBJ_STR("TSLoad Core"));
		tsobj_add_string(parent, TSOBJ_STR("path"), 
						tsobj_str_create(plat_execpath()));
	}
}

tsobj_node_t* tsobj_params_format_helper(tsload_param_t* params) {
	tsobj_node_t* params_node = tsobj_new_node(NULL);
	tsobj_node_t* param_node;
	
	tsload_param_t* param = params;	
	while(param->type != RV_PARAM_NULL) {		
		switch(param->type & TSLOAD_PARAM_MASK) {
		case TSLOAD_PARAM_INTEGER:
			param_node = tsobj_new_node("tsload.param.IntegerParam");
			break;
		case TSLOAD_PARAM_FLOAT:
			param_node = tsobj_new_node("tsload.param.FloatParam");
			break;
		case TSLOAD_PARAM_BOOLEAN:
			param_node = tsobj_new_node("tsload.param.BooleanParam");
			break;
		case TSLOAD_PARAM_STRING:
			param_node = tsobj_new_node("tsload.param.StringParam");
			break;
		case TSLOAD_PARAM_RANDGEN:
			param_node = tsobj_new_node("tsload.param.RandGenParam");
			break;
		case TSLOAD_PARAM_RANDVAR:
			param_node = tsobj_new_node("tsload.param.RandVarParam");
			break;
		default:
			++param;
			continue;
		}
		
		if(param->type & TSLOAD_PARAM_ARRAY_FLAG) 
			tsobj_add_boolean(param_node, TSOBJ_STR("array"), B_TRUE);
		else if(param->type & TSLOAD_PARAM_MAP_FLAG) 
			tsobj_add_boolean(param_node, TSOBJ_STR("map"), B_TRUE);
		
		tsobj_add_string(param_node, TSOBJ_STR("hint"), tsobj_str_create(param->hint));		
		
		tsobj_add_node(params_node, tsobj_str_create(param->name), param_node);			
		
		++param;
	}
	
	return params_node;
}

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

void* tsload_walk_random_generators(tsload_walk_op_t op, void* arg, hm_walker_func walker) {
	return tsload_walkie_talkie(op, arg, walker, &randgen_hash_map,
								(hm_tsobj_formatter) tsobj_randgen_class_format, NULL);
}

void* tsload_walk_random_variators(tsload_walk_op_t op, void* arg, hm_walker_func walker) {
	return tsload_walkie_talkie(op, arg, walker, &randvar_hash_map,
								(hm_tsobj_formatter) tsobj_randvar_class_format, NULL);
}

void* tsload_walk_tp_dispatchers(tsload_walk_op_t op, void* arg, hm_walker_func walker) {
	return tsload_walkie_talkie(op, arg, walker, &tpd_hash_map,
								(hm_tsobj_formatter) tsobj_tpd_class_format, NULL);
}

void* tsload_walk_rqsched_variators(tsload_walk_op_t op, void* arg, hm_walker_func walker) {
	return tsload_walkie_talkie(op, arg, walker, &rqsvar_hash_map,
								(hm_tsobj_formatter) tsobj_rqsvar_class_format, NULL);
}

void* tsload_walk_request_schedulers(tsload_walk_op_t op, void* arg, hm_walker_func walker) {
	return tsload_walkie_talkie(op, arg, walker, &rqsched_hash_map,
								(hm_tsobj_formatter) tsobj_rqsched_class_format, NULL);
}

/**
 * Walk over workload types registered within TSLoad
 */
void* tsload_walk_workload_types(tsload_walk_op_t op, void* arg, hm_walker_func walker) {
	return tsload_walkie_talkie(op, arg, walker, &wl_type_hash_map,
								tsobj_wl_type_format, NULL);
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
 * Mark workload as finished so it will be stopped on the next loop
 * 
 * @param wl_name name of workload
 */
int tsload_stop_workload(const char* wl_name) {
    workload_t* wl = wl_search(wl_name);

    if(wl == NULL) {
        tsload_error_msg(TSE_NOT_FOUND,
                         TSLOAD_UNCONFIGURE_WORKLOAD_ERROR_PREFIX "workload not found", wl_name);
        return TSLOAD_ERROR;
    }
    
    wl_stop(wl);
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

int tsload_start(const char* basename) {
	logmsg(LOG_INFO, "Started %s on %s...", basename, hi_get_nodename());
	logmsg(LOG_INFO, "Clock resolution is %"PRItm, tm_get_clock_res());

	/*Wait until universe collapses or we receive a signal :)*/
	t_eternal_wait();

	/*NOTREACHED*/
	return 0;
}


