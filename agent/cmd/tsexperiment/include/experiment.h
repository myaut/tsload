
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

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



#ifndef EXPERIMENT_H_
#define EXPERIMENT_H_

#include <tsload/defs.h>

#include <tsload/hashmap.h>
#include <tsload/pathutil.h>

#include <tsload/json/json.h>

#include <tsfile.h>

#include <tsload/load/threadpool.h>
#include <tsload/load/workload.h>


#define EWLHASHSIZE			8
#define EWLHASHMASK			3
#define ETPHASHSIZE			8
#define ETPHASHMASK			3

#define EXPERIMENT_ROOT		-1


/**
 * Experiment config functions return values
 */
#define EXP_CONFIG_OK				0
#define EXP_CONFIG_DUPLICATE		-1
#define EXP_CONFIG_NOT_FOUND		-2
#define EXP_CONFIG_INVALID_NODE		-3
#define EXP_CONFIG_INVALID_VALUE	-4
#define EXP_CONFIG_BAD_JSON			-5

/**
 * Experiment walker return values
 * @value EXP_WALK_CONTINUE Continue walking - go deeper if possible
 * @value EXP_WALK_NEXT Continue walking - go to next child
 * @value EXP_WALK_BREAK Stop walking and return nothing
 * @value EXP_WALK_RETURN Stop walking and return current value
 */
#define EXP_WALK_CONTINUE	0
#define EXP_WALK_RETURN		1
#define EXP_WALK_BREAK		2
#define EXP_WALK_NEXT		3

/**
 * Experiment open flags ('opening' includes opening both config and
 * trace files)
 * */
#define EXP_OPEN_NONE			0
#define EXP_OPEN_CREATE			0x01
#define EXP_OPEN_RQPARAMS		0x02
#define EXP_OPEN_SCHEMA_READ	0x04

/**
 * Experiment error code:
 * 0x 1x yy
 * 		1 - means that experiment-specific error
 *		2 - failed operation
 *		3 - error code
 */
// NOTE: if you change that, update experr.c too
#define EXPERIMENT_OK				0
// [WKE] Well-known errors
#define EXPERR_NOT_EXISTS			0x1
#define EXPERR_NO_PERMS				0x2
#define EXPERR_ALREADY_EXISTS		0x3
#define EXPERR_IS_ROOT				0x4
// [STAGE] Classes/stages for experiment error
#define EXPERR_LOAD_DIR				1
#define EXPERR_LOAD_CFG				2
#define EXPERR_PROC_CFG				3
#define EXPERR_CREATE				4
#define EXPERR_WRITE				5
#define EXPERR_MKDIR				6
#define EXPERR_OPENLOG				7
#define EXPERR_OPEN_WL				8
#define EXPERR_OPEN_WL_SCHEMA		9
#define EXPERR_OPEN_WL_TSFILE		10
#define EXPERR_STEPS				11
#define EXPERR_STEP_PROVIDE			12
#define EXPERR_RUN					13
// Macro that generates experiment error code
// G means generic error, E - extended error
#define EXPERRG(stage, code)			(0x4000 | (stage << 8) | code)
#define EXPERRE(stage, code)			(0x4000 | (stage << 8) | 0x80 | code)
// [ERRS]
// experiment_load_*
#define EXPERR_LOAD_OK				0
#define EXPERR_LOAD_NO_DIR			EXPERRG(EXPERR_LOAD_DIR, EXPERR_NOT_EXISTS)
#define EXPERR_LOAD_DIR_NO_PERMS	EXPERRG(EXPERR_LOAD_DIR, EXPERR_NO_PERMS)
#define EXPERR_LOAD_NO_FILE	  		EXPERRG(EXPERR_LOAD_CFG, EXPERR_NOT_EXISTS)
#define EXPERR_LOAD_FILE_NO_PERMS	EXPERRG(EXPERR_LOAD_CFG, EXPERR_NO_PERMS)
#define EXPERR_LOAD_FILE_ERROR 		EXPERRE(EXPERR_LOAD_CFG, 1)
#define EXPERR_LOAD_BAD_JSON		EXPERRE(EXPERR_LOAD_CFG, 2)
// experiment_process_config()
#define EXPERR_PROC_OK				0
#define EXPERR_PROC_NO_WLS			EXPERRE(EXPERR_PROC_CFG, 1)
#define EXPERR_PROC_NO_TPS			EXPERRE(EXPERR_PROC_CFG, 2)
#define EXPERR_PROC_NO_STEPS		EXPERRE(EXPERR_PROC_CFG, 3)
#define EXPERR_PROC_DUPLICATE_WL	EXPERRE(EXPERR_PROC_CFG, 4)
#define EXPERR_PROC_DUPLICATE_TP	EXPERRE(EXPERR_PROC_CFG, 5)
#define EXPERR_PROC_INVALID_WL		EXPERRE(EXPERR_PROC_CFG, 6)
#define EXPERR_PROC_INVALID_TP		EXPERRE(EXPERR_PROC_CFG, 7)
#define EXPERR_PROC_NO_STEP_WL		EXPERRE(EXPERR_PROC_CFG, 8)
#define EXPERR_PROC_WL_NO_TP		EXPERRE(EXPERR_PROC_CFG, 9)
// experiment_open_workloads()
#define EXPERR_OPEN_OK					0
#define EXPERR_OPEN_IS_ROOT				EXPERRG(EXPERR_OPEN_WL, EXPERR_IS_ROOT)
#define EXPERR_OPEN_NO_DIR				EXPERRG(EXPERR_OPEN_WL, EXPERR_NOT_EXISTS)
#define EXPERR_OPEN_DIR_NO_PERMS		EXPERRG(EXPERR_OPEN_WL, EXPERR_NO_PERMS)
#define EXPERR_OPEN_NO_WORKLOADS		EXPERRE(EXPERR_OPEN_WL, 1)
#define EXPERR_OPEN_NO_SCHEMA_FILE		EXPERRG(EXPERR_OPEN_WL_SCHEMA, EXPERR_NOT_EXISTS)
#define EXPERR_OPEN_SCHEMA_NO_PERMS		EXPERRG(EXPERR_OPEN_WL_SCHEMA, EXPERR_NO_PERMS)
#define EXPERR_OPEN_SCHEMA_EXISTS		EXPERRG(EXPERR_OPEN_WL_SCHEMA, EXPERR_ALREADY_EXISTS)
#define EXPERR_OPEN_SCHEMA_NO_WLTYPE	EXPERRE(EXPERR_OPEN_WL_SCHEMA, 1)
#define EXPERR_OPEN_SCHEMA_OPEN_ERROR	EXPERRE(EXPERR_OPEN_WL_SCHEMA, 2)
#define EXPERR_OPEN_SCHEMA_CLONE_ERROR	EXPERRE(EXPERR_OPEN_WL_SCHEMA, 3)
#define EXPERR_OPEN_SCHEMA_WRITE_ERROR	EXPERRE(EXPERR_OPEN_WL_SCHEMA, 4)
#define EXPERR_OPEN_NO_TSF_FILE			EXPERRG(EXPERR_OPEN_WL_TSFILE, EXPERR_NOT_EXISTS)
#define EXPERR_OPEN_TSF_NO_PERMS		EXPERRG(EXPERR_OPEN_WL_TSFILE, EXPERR_NO_PERMS)
#define EXPERR_OPEN_TSF_EXISTS			EXPERRG(EXPERR_OPEN_WL_TSFILE, EXPERR_ALREADY_EXISTS)
#define EXPERR_OPEN_ERROR_TSFILE		EXPERRE(EXPERR_OPEN_WL_TSFILE, 1)
// experiment_create()
#define EXPERR_CREATE_OK			0
#define EXPERR_CREATE_ALLOC_RUNID 		EXPERRE(EXPERR_CREATE, 1)
#define EXPERR_CREATE_UNNAMED			EXPERRE(EXPERR_CREATE, 2)
#define EXPERR_CREATE_SINGLE_RUN		EXPERRE(EXPERR_CREATE, 3)
// experiment_write()
#define EXPERR_WRITE_OK				0
#define EXPERR_WRITE_NO_DIR			EXPERRG(EXPERR_WRITE, EXPERR_NOT_EXISTS)
#define EXPERR_WRITE_DIR_NO_PERMS	EXPERRG(EXPERR_WRITE, EXPERR_NO_PERMS)
#define EXPERR_WRITE_RENAME_ERROR	EXPERRE(EXPERR_WRITE, 1)
#define EXPERR_WRITE_FILE_ERROR 	EXPERRE(EXPERR_WRITE, 2)
#define EXPERR_WRITE_FILE_JSON_FAIL EXPERRE(EXPERR_WRITE, 3)
// experiment_mkdir()
#define EXPERR_MKDIR_OK				0
#define EXPERR_MKDIR_IS_ROOT		EXPERRG(EXPERR_MKDIR, EXPERR_IS_ROOT)
#define EXPERR_MKDIR_EXISTS			EXPERRG(EXPERR_MKDIR, EXPERR_ALREADY_EXISTS)
#define EXPERR_MKDIR_NO_PERMS		EXPERRG(EXPERR_MKDIR, EXPERR_NO_PERMS)
#define EXPERR_MKDIR_ERROR			EXPERRE(EXPERR_MKDIR, 1)
// experiment_open_log()
#define EXPERR_OPENLOG_OK			0
#define EXPERR_OPENLOG_IS_ROOT		EXPERRG(EXPERR_OPENLOG, EXPERR_IS_ROOT)
#define EXPERR_OPENLOG_NO_DIR		EXPERRG(EXPERR_OPENLOG, EXPERR_NOT_EXISTS)
#define EXPERR_OPENLOG_DIR_NO_PERMS	EXPERRG(EXPERR_OPENLOG, EXPERR_NO_PERMS)
#define EXPERR_OPENLOG_EXISTS		EXPERRG(EXPERR_OPENLOG, EXPERR_ALREADY_EXISTS)
#define EXPERR_OPENLOG_ERROR		EXPERRE(EXPERR_OPENLOG, 1)
// experiment_create_steps()
#define EXPERR_STEPS_OK						0
#define EXPERR_STEPS_MISSING				EXPERRE(EXPERR_STEPS, 1)
#define EXPERR_STEPS_INVALID_FILE			EXPERRE(EXPERR_STEPS, 2)
#define EXPERR_STEPS_FILE_ERROR				EXPERRE(EXPERR_STEPS, 3)
#define EXPERR_STEPS_SERIES_ERROR			EXPERRE(EXPERR_STEPS, 10)
#define EXPERR_STEPS_INVALID_CONST			EXPERRE(EXPERR_STEPS, 4)
#define EXPERR_STEPS_INVALID_TRACE			EXPERRE(EXPERR_STEPS, 5)
#define EXPERR_STEPS_END_OF_TRACE			EXPERRE(EXPERR_STEPS, 6)
#define EXPERR_STEPS_TRACE_TSFILE_ERROR		EXPERRE(EXPERR_STEPS, 7)
#define EXPERR_STEPS_TRACE_REQUEST_ERROR	EXPERRE(EXPERR_STEPS, 8)
#define EXPERR_STEPS_TRACE_CHAINED_ERROR	EXPERRE(EXPERR_STEPS, 9)
// tse_run_wl_provide_step()
#define EXPERR_STEP_PROVIDE_OK				0
#define EXPERR_STEP_PROVIDE_STEP_ERROR		EXPERRE(EXPERR_STEP_PROVIDE, 1)
#define EXPERR_STEP_PROVIDE_TSLOAD_ERROR	EXPERRE(EXPERR_STEP_PROVIDE, 2)
#define EXPERR_STEP_PROVIDE_QFULL			EXPERRE(EXPERR_STEP_PROVIDE, 3)
// experiment_run()
#define EXPERR_RUN_OK				0
#define EXPERR_RUN_IS_ROOT			EXPERRG(EXPERR_RUN, EXPERR_IS_ROOT)
#define EXPERR_RUN_ALREADY_RUNNING	EXPERRE(EXPERR_RUN, 1)
#define EXPERR_RUN_TP_ERROR			EXPERRE(EXPERR_RUN, 2)
#define EXPERR_RUN_WL_ERROR			EXPERRE(EXPERR_RUN, 3)
#define EXPERR_RUN_SCHED_ERROR		EXPERRE(EXPERR_RUN, 4)

/**
 * Experiment status codes
 */
#define EXPERIMENT_FINISHED				2
#define EXPERIMENT_NOT_CONFIGURED		1
#define EXPERIMENT_OK					0
#define EXPERIMENT_ERROR				-1
#define EXPERIMENT_UNKNOWN				-2
#define EXPERIMENT_INTERRUPTED          -3

#define EXPERIMENT_LOG_FILE		"tsexperiment.out"
#define EXPERIMENT_FILENAME		"experiment.json"
#define EXPERIMENT_FILENAME_OLD	"experiment.json.old"
#define EXPID_FILENAME			"experiment.id"
#define EXP_SCHEMA_SUFFIX		"schema.json"

struct steps_generator;

typedef struct exp_threadpool {
	/* Config parameters */
	AUTOSTRING char* tp_name;

	unsigned tp_num_threads;
	ts_time_t tp_quantum;
	boolean_t tp_discard;

	json_node_t* tp_disp;
	json_node_t* tp_sched;

	struct exp_threadpool* tp_next;

	int tp_status;
} exp_threadpool_t;

typedef struct exp_workload {
	/* Config parameters */
	AUTOSTRING char* wl_name;

	AUTOSTRING char* wl_type;
	AUTOSTRING char* wl_tp_name;

	ts_time_t wl_deadline;

	json_node_t* wl_chain;
	json_node_t* wl_rqsched;
	json_node_t* wl_params;

	AUTOSTRING char* wl_chain_name;
	json_node_t* wl_steps_cfg;

	/* Current parameters */
	struct steps_generator* wl_steps;

	exp_threadpool_t* wl_tp;

	boolean_t wl_is_chained;

	tsfile_schema_t* wl_file_schema;
	tsfile_t* wl_file;
	int wl_file_flags;
	int wl_file_index;	/* Cached value for trace-based generator */

	struct exp_workload* wl_chain_next;
	struct exp_workload* wl_next;

	int wl_status;		/* EXPERIMENT_OK etc. - not a wl_notify() status */
} exp_workload_t;

/* No need to pack/pad this structure because we generate schema dynamically.  */
typedef struct exp_request_entry {
	uint32_t	rq_step;
	int32_t		rq_request;
	int32_t 	rq_chain_request;
	int32_t		rq_thread;
	int32_t		rq_user;
	int64_t		rq_sched_time;
	int64_t		rq_start_time;
	int64_t		rq_end_time;
	int32_t		rq_queue_length;
	uint16_t	rq_flags;
} exp_request_entry_t;

/**
 * Experiment
 *
 * @member exp_name name of experiment. Read from experiment subdir, \
 * 		but after config processed from "name" node of config
 * @member exp_root root path of experiment
 * @member exp_directory subdir of experiment run. For current experiment config	\
 * 		set to empty string
 * @member exp_basedir base directory used in experiment_create()
 * @member exp_runid id of experiment run. Set to EXPERIMENT_ROOT for current config
 * @member exp_single_run for single run experiments set to true
 * @member exp_config experiment config
 * @member exp_threadpools dynamic hashmap of threadpools. Created by experiment_process_config()
 * @member exp_workloads  dynamic hashmap of workloads. Created by experiment_process_config()
 */
typedef struct {
	AUTOSTRING char* exp_name;

	char exp_root[PATHMAXLEN];
	char exp_directory[PATHPARTMAXLEN];
	char exp_basedir[PATHPARTMAXLEN];

	int exp_runid;
	boolean_t exp_single_run;
	json_node_t* exp_config;

	hashmap_t* exp_threadpools;
	hashmap_t* exp_workloads;

	FILE* exp_log;

	thread_mutex_t exp_mutex;
	thread_cv_t exp_cv;
	int exp_wl_count;
	int exp_wl_configuring_count;
	int exp_wl_running_count;

	int exp_status;

	int exp_error;

	boolean_t exp_trace_mode;
} experiment_t;

experiment_t* experiment_load_root(const char* path);
experiment_t* experiment_load_dir(const char* root_path, int runid, const char* dir);
experiment_t* experiment_load_run(experiment_t* root, int runid);
experiment_t* experiment_load_single_run(experiment_t* root);
unsigned experiment_load_error();

experiment_t* experiment_create(experiment_t* root, experiment_t* exp, const char* name);
unsigned experiment_create_error();

void experiment_destroy(experiment_t* exp);

struct experiment_walk_ctx {
	experiment_t* root;
	int runid;
	const char* name;
	const char* dir;
};

typedef int (*experiment_walk_func)(struct experiment_walk_ctx* ctx, void* context);
typedef void (*experiment_noent_func)(void* context);

experiment_t* experiment_walk(experiment_t* root, experiment_walk_func pred, void* context, 
							  experiment_noent_func noent);

int experiment_mkdir(experiment_t* exp, experiment_t* root);
int experiment_write(experiment_t* exp);

typedef int (*experiment_cfg_walk_func)(const char* name, json_node_t* parent, json_node_t* node, void* context);

json_node_t* experiment_cfg_find(json_node_t* config, const char* name, json_node_t** parent, json_type_t type);
json_node_t* experiment_cfg_walk(json_node_t* config, experiment_cfg_walk_func pre, experiment_cfg_walk_func post, void* context);
int experiment_cfg_set(json_node_t* config, const char* name, const char* value);
int experiment_cfg_add(json_node_t* config, const char* parent_name, json_str_t name,
					   json_node_t* node, boolean_t replace);

int experiment_process_config(experiment_t* exp);
int experiment_open_workloads(experiment_t* exp, int flags);
int experiment_open_log(experiment_t* exp);

#endif /* EXPERIMENT_H_ */


