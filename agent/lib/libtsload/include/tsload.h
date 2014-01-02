/*
 * tsload.h
 *
 *  Created on: Dec 3, 2012
 *      Author: myaut
 */

#ifndef TSLOAD_H_
#define TSLOAD_H_

#include <defs.h>
#include <list.h>
#include <errcode.h>

#include <tsinit.h>
#include <tstime.h>

#include <libjson.h>

#define TSLOAD_ERROR	1
#define TSLOAD_OK		0

typedef void (*tsload_error_msg_func)(ts_errcode_t code, const char* format, ...);
typedef void (*tsload_workload_status_func)(const char* wl_name,
					 					    int status,
										    long progress,
										    const char* config_msg);
typedef void (*tsload_requests_report_func)(list_head_t* rq_list);

LIBIMPORT tsload_error_msg_func tsload_error_msg;
LIBIMPORT tsload_workload_status_func tsload_workload_status;
LIBIMPORT tsload_requests_report_func tsload_requests_report;

/* TSLoad calls */
LIBEXPORT JSONNODE* tsload_get_workload_types(void);
LIBEXPORT JSONNODE* tsload_get_resources(void);

LIBEXPORT int tsload_configure_workload(const char* wl_name, JSONNODE* wl_params);
LIBEXPORT int tsload_provide_step(const char* wl_name, long step_id, unsigned num_rqs, int* pstatus);
LIBEXPORT int tsload_start_workload(const char* wl_name, ts_time_t start_time);
LIBEXPORT int tsload_unconfigure_workload(const char* wl_name);

LIBEXPORT int tsload_create_threadpool(const char* tp_name, unsigned num_threads, ts_time_t quantum, const char* disp_name);
LIBEXPORT JSONNODE* tsload_get_threadpools(void);
LIBEXPORT JSONNODE* tsload_get_dispatchers(void);
LIBEXPORT int tsload_destroy_threadpool(const char* tp_name);

LIBEXPORT int tsload_init(struct subsystem* pre_subsys, unsigned pre_count,
						  struct subsystem* post_subsys, unsigned post_count);
LIBEXPORT int tsload_start(const char* basename);

#endif /* TSLOAD_H_ */
