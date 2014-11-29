
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



#ifndef TSLOAD_H_
#define TSLOAD_H_

#include <tsload/defs.h>

#include <tsload/list.h>
#include <tsload/errcode.h>
#include <tsload/hashmap.h>
#include <tsload/init.h>
#include <tsload/time.h>

#include <tsload/obj/obj.h>

#include <tsload/load/workload.h>


/**
 * @module TSLoad high-level API
 *
 * Use this API to build custom agent on top of libtsload
 */

/**
 * TSLoad error codes
 */
#define TSLOAD_ERROR	1
#define TSLOAD_OK		0

/**
 * Hook that called when one of high-level operations encounters errors.
 * May be called multiple times during one call
 */
typedef void (*tsload_error_msg_func)(ts_errcode_t code, const char* format, ...);
LIBIMPORT tsload_error_msg_func tsload_error_msg;

/**
 * Hook for reporting workload statuses
 */
typedef void (*tsload_workload_status_func)(const char* wl_name,
					 					    int status,
										    long progress,
										    const char* config_msg);
LIBIMPORT tsload_workload_status_func tsload_workload_status;

/**
 * Hook for reporting requests and it's params
 */
typedef void (*tsload_requests_report_func)(list_head_t* rq_list);
LIBIMPORT tsload_requests_report_func tsload_requests_report;

/* TSLoad calls */

/**
 * Walkie-talkie. Operations for tsload_walk_* functions
 *
 * @value TSLOAD_WALK_FIND Find element identified by key stored in arg
 * @value TSLOAD_WALK_JSON Do TSLOAD_WALK_FIND but return formatted JSONNODE*
 * @value TSLOAD_WALK_JSON_ALL Walks over hashmap and return json representation of all items. args are ignored
 * @value TSLOAD_WALK_WALK Walk over hashmap traditionally. arg - context of the walker
 */
typedef enum tsload_walk_op {
	TSLOAD_WALK_FIND,
	TSLOAD_WALK_TSOBJ,
	TSLOAD_WALK_TSOBJ_ALL,
	TSLOAD_WALK_WALK
} tsload_walk_op_t;

LIBEXPORT void* tsload_walk_workload_types(tsload_walk_op_t op, void* arg, hm_walker_func walker);

LIBEXPORT tsobj_node_t* tsload_get_resources(void);
LIBEXPORT tsobj_node_t* tsload_get_hostinfo(void);

LIBEXPORT int tsload_configure_workload(const char* wl_name, const char* wl_type, const char* tp_name, ts_time_t deadline,
										tsobj_node_t* wl_chain_params, tsobj_node_t* rqsched_params, tsobj_node_t* wl_params);
LIBEXPORT int tsload_provide_step(const char* wl_name, long step_id, unsigned num_rqs, list_head_t* trace_rqs,
								  int* pstatus);
LIBEXPORT int tsload_create_request(const char* wl_name, list_head_t* rq_list, boolean_t chained,
	 	   	   	   	   	   	   	   	int rq_id, long step, int user_id, int thread_id,
	 	   	   	   	   	   	   	   	ts_time_t sched_time, void* rq_params);
LIBEXPORT int tsload_start_workload(const char* wl_name, ts_time_t start_time);
LIBEXPORT int tsload_unconfigure_workload(const char* wl_name);

LIBEXPORT int tsload_create_threadpool(const char* tp_name, unsigned num_threads, ts_time_t quantum,
		 	 	 	 	 	 	 	   boolean_t discard, tsobj_node_t* disp);
LIBEXPORT int tsload_schedule_threadpool(const char* tp_name, tsobj_node_t* sched);
LIBEXPORT void* tsload_walk_threadpools(tsload_walk_op_t op, void* arg, hm_walker_func walker);
LIBEXPORT int tsload_destroy_threadpool(const char* tp_name);

LIBEXPORT int tsload_init(struct subsystem* pre_subsys, unsigned pre_count,
						  struct subsystem* post_subsys, unsigned post_count);
LIBEXPORT int tsload_start(const char* basename);

#endif /* TSLOAD_H_ */

