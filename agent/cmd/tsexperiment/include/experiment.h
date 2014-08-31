/*
 * experiment.h
 *
 *  Created on: Feb 8, 2014
 *      Author: myaut
 */

#ifndef EXPERIMENT_H_
#define EXPERIMENT_H_

#include <defs.h>
#include <hashmap.h>
#include <pathutil.h>
#include <tsfile.h>
#include <threads.h>

#include <threadpool.h>
#include <workload.h>

#include <json.h>

#define EWLHASHSIZE			8
#define EWLHASHMASK			3
#define ETPHASHSIZE			8
#define ETPHASHMASK			3

#define EXPERIMENT_ROOT		-1

#define EXPNAMELEN			128
#define EXPERRLEN			256

#define EXP_LOAD_OK				0
#define EXP_LOAD_ERR_OPEN_FAIL  -1
#define EXP_LOAD_ERR_BAD_JSON	-2

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

#define EXP_OPEN_NONE			0
#define EXP_OPEN_CREATE			0x01
#define EXP_OPEN_RQPARAMS		0x02
#define EXP_OPEN_SCHEMA_READ	0x04

#define EXP_OPEN_OK				0
#define EXP_OPEN_NO_WORKLOADS	-1
#define EXP_OPEN_BAD_SCHEMA		-2
#define EXP_OPEN_ERROR_SCHEMA	-3
#define EXP_OPEN_ERROR_TSFILE	-4

#define EXPERIMENT_WRITE_OK					0
#define EXPERIMENT_WRITE_RENAME_ERROR		-1
#define EXPERIMENT_WRITE_FILE_ERROR 		-2

#define EXPERIMENT_MKDIR_OK			0
#define EXPERIMENT_MKDIR_INVALID	-1
#define EXPERIMENT_MKDIR_EXISTS		-2
#define EXPERIMENT_MKDIR_ERROR		-3

#define EXPERIMENT_OPENLOG_OK			0
#define EXPERIMENT_OPENLOG_INVALID		-1
#define EXPERIMENT_OPENLOG_EXISTS		-2
#define EXPERIMENT_OPENLOG_ERROR		-3

#define EXPERIMENT_FINISHED				2
#define EXPERIMENT_NOT_CONFIGURED		1
#define EXPERIMENT_OK					0
#define EXPERIMENT_ERROR				-1
#define EXPERIMENT_UNKNOWN				-2

#define EXPERIMENT_LOG_FILE		"tsexperiment.out"
#define EXPERIMENT_FILENAME		"experiment.json"
#define EXPERIMENT_FILENAME_OLD	"experiment.json.old"
#define EXPID_FILENAME			"experiment.id"
#define EXP_SCHEMA_SUFFIX		"schema.json"

struct steps_generator;

typedef struct exp_threadpool {
	/* Config parameters */
	char tp_name[TPNAMELEN];

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
	char wl_name[WLNAMELEN];

	char wl_type[WLTNAMELEN];
	char wl_tp_name[TPNAMELEN];

	ts_time_t wl_deadline;

	json_node_t* wl_chain;
	json_node_t* wl_rqsched;
	json_node_t* wl_params;

	char wl_chain_name[WLNAMELEN];
	json_node_t* wl_steps_cfg;

	/* Current parameters */
	struct steps_generator* wl_steps;

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
 * @member exp_config experiment config
 * @member exp_threadpools dynamic hashmap of threadpools. Created by experiment_process_config()
 * @member exp_workloads  dynamic hashmap of workloads. Created by experiment_process_config()
 */
typedef struct {
	char exp_name[EXPNAMELEN];

	char exp_root[PATHMAXLEN];
	char exp_directory[PATHPARTMAXLEN];
	char exp_basedir[PATHPARTMAXLEN];

	int exp_runid;
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
	char exp_error_msg[EXPERRLEN];

	boolean_t exp_trace_mode;
} experiment_t;

experiment_t* experiment_load_root(const char* path);
experiment_t* experiment_load_dir(const char* root_path, int runid, const char* dir);
experiment_t* experiment_load_run(experiment_t* root, int runid);
experiment_t* experiment_create(experiment_t* root, experiment_t* exp, const char* name);

void experiment_destroy(experiment_t* exp);

struct experiment_walk_ctx {
	experiment_t* root;
	int runid;
	const char* name;
	const char* dir;
};

typedef int (*experiment_walk_func)(struct experiment_walk_ctx* ctx, void* context);

experiment_t* experiment_walk(experiment_t* root, experiment_walk_func pred, void* context);

int experiment_mkdir(experiment_t* exp);
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
