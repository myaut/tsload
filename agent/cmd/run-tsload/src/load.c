/*
 * load.c
 *
 *  Created on: Nov 19, 2012
 *      Author: myaut
 */

#define LOG_SOURCE "load"
#include <log.h>

#include <defs.h>
#include <filemmap.h>
#include <threads.h>
#include <hashmap.h>
#include <atomic.h>
#include <mempool.h>
#include <workload.h>
#include <tstime.h>
#include <pathutil.h>
#include <plat/posixdecl.h>
#include <tsfile.h>
#include <tsdirent.h>
#include <threadpool.h>
#include <tpdisp.h>

#include <tsload.h>

#include <commands.h>
#include <steps.h>

#include <libjson.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>

char experiment_dir[PATHMAXLEN];

char experiment_filename[PATHMAXLEN];
char rqreport_filename[PATHMAXLEN];

char* exp_name;

LIBIMPORT char log_filename[];

JSONNODE* experiment = NULL;

char experiment_name[EXPNAMELEN];

/* Wait until experiment finishes or fails */
static thread_mutex_t	output_lock;
static thread_event_t	workload_event;
static thread_event_t	request_event;

typedef struct agent_workload {
	char wl_name[WLNAMELEN];

	steps_generator_t* wl_steps;
	boolean_t 		   wl_is_chained;
	tsfile_t* 		   wl_rqreport_file;
	tsfile_schema_t*   wl_schema;

	struct agent_workload* wl_hm_next;
} agent_workload_t;

DECLARE_HASH_MAP(agent_wl_hash_map, agent_workload_t, WLHASHSIZE, wl_name, wl_hm_next,
	{
		return hm_string_hash(key, WLHASHMASK);
	},
	{
		return strcmp((char*) key1, (char*) key2) == 0;
	}
)

static atomic_t	wl_cfg_count;
static atomic_t	wl_run_count;
static atomic_t	rq_count;

static char**	tp_name_array = NULL;	/* Names of configured threadpools */
static int		tp_count = 0;

static boolean_t load_error = B_FALSE;

/* No need to pack/pad this structure because we generate schema dynamically.  */
struct request_entry {
	uint32_t	step;
	uint32_t	request;
	uint32_t	thread;
	int64_t		sched_time;
	int64_t		start_time;
	int64_t		end_time;
	uint16_t	flags;
};

tsfile_schema_t request_schema = {
	TSFILE_SCHEMA_HEADER(sizeof(struct request_entry), 7),
	{
		TSFILE_SCHEMA_FIELD_REF(struct request_entry, step, TSFILE_FIELD_INT),
		TSFILE_SCHEMA_FIELD_REF(struct request_entry, request, TSFILE_FIELD_INT),
		TSFILE_SCHEMA_FIELD_REF(struct request_entry, thread, TSFILE_FIELD_INT),
		TSFILE_SCHEMA_FIELD_REF(struct request_entry, sched_time, TSFILE_FIELD_INT),
		TSFILE_SCHEMA_FIELD_REF(struct request_entry, start_time, TSFILE_FIELD_INT),
		TSFILE_SCHEMA_FIELD_REF(struct request_entry, end_time, TSFILE_FIELD_INT),
		TSFILE_SCHEMA_FIELD_REF(struct request_entry, flags, TSFILE_FIELD_INT),
	}
};

const char* wl_status_msg(wl_status_t wls) {
	switch(wls) {
	case WLS_CONFIGURING:
		return "configuring";
	case WLS_CONFIGURED:
		return "successfully configured";
	case WLS_CFG_FAIL:
		return "configuration failed";
	case WLS_STARTED:
		return "started";
	case WLS_FINISHED:
		return "finished";
	case WLS_RUNNING:
		return "running";
	}

	return "";
}

/**
 * MT-safe implementation for vfprintf */
static int load_vfprintf(FILE* file, const char* fmt, va_list va) {
	int ret;

	mutex_lock(&output_lock);
	ret = vfprintf(file, fmt, va);
	mutex_unlock(&output_lock);

	return ret;
}

/**
 * @see load_vfprintf */
static int load_fprintf(FILE* file, const char* fmt, ...) {
	va_list va;
	int ret;

	va_start(va, fmt);
	ret = load_vfprintf(file, fmt, va);
	va_end(va);

	return ret;
}

/**
 * Reads JSON representation of experiment from file and parses it
 *
 * @return LOAD_OK if everything was OK or LOAD_ERR_ constant
 */
static int experiment_read() {
	int ret = LOAD_OK;
	char* data;
	size_t length;
	FILE* file = fopen(experiment_filename, "r");

	if(!file) {
		logmsg(LOG_CRIT, "Couldn't open workload file %s", experiment_filename);
		return LOAD_ERR_OPEN_FAIL;
	}

	/* Get length of file than rewind */
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, 0, SEEK_SET);

	/* Read data from file */
	data = mp_malloc(length + 1);
	fread(data, length, 1, file);
	data[length] = '\0';

	/* Parse JSON */
	experiment = json_parse(data);

	mp_free(data);
	fclose(file);

	if(experiment == NULL) {
		logmsg(LOG_CRIT, "JSON parse error of file %s", experiment_filename);
		ret = LOAD_ERR_BAD_JSON;
	}

	return ret;
}

static void unconfigure_all_wls(void);

/**
 * Reads steps file name from configuration for workload wl_name; opens
 * steps file or creates const generator.
 * If open encounters an error or invalid description provided,
 * returns LOAD_ERR_CONFIGURE otherwise returns LOAD_OK */
static steps_generator_t* load_steps(const char* wl_name, JSONNODE* steps_node) {
	int ret = LOAD_OK;

	JSONNODE_ITERATOR	i_step, i_end, i_file, i_const_rqs, i_const_steps;
	char* step_fn;
	char step_filename[PATHMAXLEN];

	long num_steps;
	unsigned num_requests;

	steps_generator_t* sg = NULL;

	i_end = json_end(steps_node);
	i_step = json_find(steps_node, wl_name);

	if( i_step == i_end ||
		json_type(*i_step) != JSON_NODE) {

			load_fprintf(stderr, "Missing steps for workload %s\n", wl_name);
			return NULL;
	}

	i_end = json_end(*i_step);
	i_file = json_find(*i_step, "file");

	if(i_file != i_end) {
		if(json_type(*i_file) != JSON_STRING) {
			load_fprintf(stderr, "Invalid steps description for workload %s: 'file' should be a string\n", wl_name);
			return NULL;
		}

		step_fn = json_as_string(*i_file);
		path_join(step_filename, PATHMAXLEN, experiment_dir, step_fn, NULL);

		sg = step_create_file(step_filename);

		if(sg != NULL) {
			load_fprintf(stderr, "Loaded steps file %s for workload %s\n", step_filename, wl_name);
		}
		else {
			load_fprintf(stderr, "Couldn't open steps file for workload %s\n", step_filename, wl_name);
		}

		json_free(step_fn);
	}
	else {
		i_const_rqs = json_find(*i_step, "num_requests");
		i_const_steps = json_find(*i_step, "num_steps");

		if(i_const_rqs != i_end && i_const_steps != i_end) {
			if(json_type(*i_const_rqs) != JSON_NUMBER ||
			   json_type(*i_const_steps) != JSON_NUMBER) {
				load_fprintf(stderr, "Invalid steps description for workload %s:"
									 "'num_requests' and 'num_steps' should be numbers\n", wl_name);
							return NULL;
			}

			num_steps = json_as_int(*i_const_steps);
			num_requests = json_as_int(*i_const_rqs);

			sg = step_create_const(num_steps, num_requests);

			load_fprintf(stderr, "Created constant steps generator for workload %s with N=%d R=%d\n",
					wl_name, num_steps, num_requests);
		}
		else {
			load_fprintf(stderr, "Not found steps parameters for workload %s\n", wl_name);
			return NULL;
		}
	}

	return sg;
}

/**
 * Read step from steps file than provide it to tsload core
 * If it encounters parse error, set load_error flag
 *
 * @return result of step_get_step*/
int load_provide_step(agent_workload_t* awl) {
	unsigned num_rqs = 0;
	long step_id = -1;
	int status;
	int ret;

	if(awl->wl_is_chained)
		return STEP_NO_RQS;

	ret = step_get_step(awl->wl_steps, &step_id, &num_rqs);

	if(ret == STEP_ERROR) {
		load_fprintf(stderr, "Cannot process step %ld: cannot parse or internal error!\n", step_id);
		load_error = B_TRUE;

		return ret;
	}

	if(ret == STEP_OK)
		tsload_provide_step(awl->wl_name, step_id, num_rqs, &status);

	atomic_add(&rq_count, num_rqs);

	return ret;
}

static agent_workload_t* awl_create(const char* wl_name) {
	agent_workload_t* awl = mp_malloc(sizeof(agent_workload_t));

	strncpy(awl->wl_name, wl_name, WLNAMELEN);
	awl->wl_hm_next = NULL;
	awl->wl_steps = NULL;
	awl->wl_is_chained = B_FALSE;
	awl->wl_rqreport_file = NULL;

	/* Should be initialized here */
	awl->wl_schema = &request_schema;

	hash_map_insert(&agent_wl_hash_map, awl);

	return awl;
}

static void awl_destroy(agent_workload_t* awl) {
	if(awl->wl_steps != NULL)
		step_destroy(awl->wl_steps);

	if(awl->wl_rqreport_file != NULL)
		tsfile_close(awl->wl_rqreport_file);

	mp_free(awl);
}

/**
 * Configure all workloads
 *
 * @param steps_node JSON node with paths to steps files
 * @param wl_node JSON node with workload parameters
 *
 * @return LOAD_ERR_CONFIGURE if error was encountered while we configured workloads
 * LOAD_OK if everything was fine.
 *
 * tsload_configure_workload doesn't provide error code, and errors are occur totally asynchronous to
 * configure_all_wls code. If it couldn't load steps or error was reported during configuration,
 * it abandons configuration and returns LOAD_ERR_CONFIGURE. If error reported after configure_all_wls
 * configures all workloads, it would be processed by do_load itself.
 * */
static int configure_all_wls(JSONNODE* steps_node, JSONNODE* wl_node) {
	JSONNODE_ITERATOR iter = json_begin(wl_node),
					  end = json_end(wl_node);
	workload_t* wl;
	char* wl_name;
	agent_workload_t* awl;

	int num = json_size(wl_node);
	int steps_err;
	int tsload_ret;

	load_fprintf(stdout, "\n\n==== CONFIGURING WORKLOADS ====\n");

	if(json_type(steps_node) != JSON_NODE) {
		load_fprintf(stderr, "Cannot process steps: not a JSON node\n");
		return LOAD_ERR_CONFIGURE;
	}

	while(iter != end) {
		wl_name = json_name(*iter);

		awl = awl_create(wl_name);

		tsload_configure_workload(wl_name, *iter);
		atomic_inc(&wl_cfg_count);

		if(load_error) {
			awl_destroy(awl);
			return LOAD_ERR_CONFIGURE;
		}

		json_free(wl_name);

		if(json_find(*iter, "chain") != json_end(*iter)) {
			load_fprintf(stdout, "Chained workload %s\n", awl->wl_name);
			awl->wl_is_chained = B_TRUE;
		}
		else {
			awl->wl_steps = load_steps(awl->wl_name, steps_node);
			if(awl->wl_steps == NULL) {
				/* Error encountered during configuration - do not configure other workloads */
				hash_map_remove(&agent_wl_hash_map, awl);
				awl_destroy(awl);
				return LOAD_ERR_CONFIGURE;
			}

			load_fprintf(stdout, "Initialized workload %s\n", awl->wl_name);
		}

		hash_map_insert(&agent_wl_hash_map, awl);

		++iter;
	}

	return LOAD_OK;
}

static int awl_unconfigure_walker(hm_item_t* obj, void* ctx) {
	agent_workload_t* awl = (agent_workload_t*) obj;

	tsload_unconfigure_workload(awl->wl_name);
	awl_destroy(awl);

	return HM_WALKER_REMOVE | HM_WALKER_CONTINUE;
}

static void unconfigure_all_wls(void) {
	hash_map_walk(&agent_wl_hash_map, awl_unconfigure_walker, NULL);
}

typedef struct awl_start_context {
	char start_time_str[32];
	ts_time_t start_time;
} awl_start_context_t;

static int awl_start_walker(hm_item_t* obj, void* ctx) {
	agent_workload_t* awl = (agent_workload_t*) obj;
	awl_start_context_t* actx = (awl_start_context_t*) ctx;
	int step;
	int ret;

	for(step = 0; step < WLSTEPQSIZE; ++step) {
		ret = load_provide_step(awl);

		if(ret == STEP_ERROR) {
			load_error = LOAD_ERR_CONFIGURE;
			return HM_WALKER_STOP;
		}

		if(ret == STEP_NO_RQS)
			break;
	}

	tsload_start_workload(awl->wl_name, actx->start_time);

	load_fprintf(stdout, "Scheduling workload %s at %s,%03ld (provided %d steps)\n", awl->wl_name,
			actx->start_time_str, TS_TIME_MS(actx->start_time), step);

	if(!awl->wl_is_chained)
		atomic_inc(&wl_run_count);

	if(load_error != LOAD_OK)
		return HM_WALKER_STOP;

	return HM_WALKER_CONTINUE;
}

/**
 * Feeds workloads with first WLSTEPQSIZE steps and schedules workloads to start
 * Workloads are starting at the same time, but  */
static void start_all_wls(void) {
	awl_start_context_t actx;
	time_t unix_start_time;

	actx.start_time  = tm_get_time() + WL_START_DELAY;
	unix_start_time = TS_TIME_TO_UNIX(actx.start_time);
	strftime(actx.start_time_str, 32, "%H:%M:%S %d.%m.%Y", localtime(&unix_start_time));

	load_fprintf(stdout, "\n\n=== RUNNING EXPERIMENT '%s' === \n", experiment_name);

	hash_map_walk(&agent_wl_hash_map, awl_start_walker, &actx);
}

#define CONFIGURE_TP_PARAM(i_node, name, type) 											\
	i_node = json_find(*iter, name);													\
	if(i_node == i_end || json_type(*i_node) != type) {								 	\
		load_fprintf(stderr, "Missing or invalid param '%s' in threadpool '%s'\n", 		\
											name, tp_name);								\
		json_free(tp_name);																\
		return LOAD_ERR_CONFIGURE;														\
	}

static int configure_threadpools(JSONNODE* tp_node) {
	JSONNODE_ITERATOR iter = json_begin(tp_node),
				      end = json_end(tp_node);
	JSONNODE_ITERATOR i_num_threads, i_quantum, i_disp, i_sched, i_discard;
	JSONNODE_ITERATOR i_end;

	unsigned num_threads;
	ts_time_t quantum;
	boolean_t discard = B_FALSE;

	char* tp_name;
	char* disp_name;
	int num = json_size(tp_node);

	int tsload_ret;
	int i;

	load_fprintf(stdout, "\n\n==== CONFIGURING THREADPOOLS ====\n");
	tp_name_array = mp_malloc(num * sizeof(char*));

	while(iter != end) {
		i_end = json_end(*iter);
		tp_name = json_name(*iter);

		CONFIGURE_TP_PARAM(i_num_threads, "num_threads", JSON_NUMBER);
		CONFIGURE_TP_PARAM(i_quantum, "quantum", JSON_NUMBER);
		CONFIGURE_TP_PARAM(i_disp, "disp", JSON_NODE);
		i_sched = json_find(*iter, "sched");
		i_discard = json_find(*iter, "discard");

		num_threads = json_as_int(*i_num_threads);
		quantum = json_as_int(*i_quantum);

		if(i_discard != i_end) {
			discard = json_as_bool(*i_discard);
		}

		tsload_ret = tsload_create_threadpool(tp_name, num_threads, quantum, discard, *i_disp);

		if(tsload_ret != TSLOAD_OK) {
			return LOAD_ERR_CONFIGURE;
		}

		if(i_sched != i_end) {
			/* If we fail to do bindings - ignore it */
			(void) tsload_schedule_threadpool(tp_name, *i_sched);
		}

		load_fprintf(stdout, "Configured thread pool '%s' with %u threads and %lld ns quantum\n",
						tp_name, num_threads, quantum);

		tp_name_array[tp_count++] = tp_name;

		++iter;
	}

	return LOAD_OK;
}

static void unconfigure_threadpools(void) {
	int i;

	for(i = 0; i < tp_count; ++i) {
		tsload_destroy_threadpool(tp_name_array[i]);
		json_free(tp_name_array[i]);
	}

	mp_free(tp_name_array);
}

static long find_experiment_id(void) {
	plat_dir_t* dir = plat_opendir(experiment_dir);
	plat_dir_entry_t* de = NULL;

	size_t exp_name_len = strlen(experiment_name);
	char* ptr = NULL;
	long max_exp_id = 0, exp_id;

	while((de = plat_readdir(dir)) != NULL) {
		if(plat_dirent_hidden(de) || plat_dirent_type(de) != DET_DIR)
			continue;

		if(strncmp(de->d_name, experiment_name, exp_name_len) != 0)
			continue;

		ptr = strchr(de->d_name, '-');
		if(ptr == NULL)
			continue;

		++ptr;
		exp_id = strtol(ptr, NULL, 10);

		if(exp_id > max_exp_id)
			max_exp_id = exp_id;
	}

	return max_exp_id;
}

/**
 * Generating experiment id. It is cached in file experiment.id.
 * If cache-file found, simply read value from that. If no such file
 * exists, walk over entire experiment directory and find latest
 * experiment id. Rewrite "experiment.id" in any case.
 */
static long generate_experiment_id(void) {
	FILE* exp_id_file;
	long exp_id = -1;

	char exp_id_path[PATHMAXLEN];
	char exp_id_str[32];

	path_join(exp_id_path, PATHMAXLEN, experiment_dir, DEFAULT_EXPID_FILENAME, NULL);

	exp_id_file = fopen(exp_id_path, "r");

	if(exp_id_file != NULL) {
		fgets(exp_id_str, 32, exp_id_file);
		exp_id = strtol(exp_id_str, NULL, 10);
		fclose(exp_id_file);
	}

	if(exp_id == -1) {
		exp_id = find_experiment_id();
	}

	++exp_id;

	exp_id_file = fopen(exp_id_path, "w");
	fprintf(exp_id_file, "%ld\n", exp_id);
	fclose(exp_id_file);

	return exp_id;
}

static void experiment_write(JSONNODE* experiment) {
	time_t now;
	char datetime[32];

	JSONNODE_ITERATOR i_name;
	JSONNODE* j_datetime;

	FILE* exp_file;
	char exp_path[PATHMAXLEN];
	char* data;

	now = time(NULL);
	strftime(datetime, 32, "%H:%M:%S %d.%m.%Y", localtime(&now));

	/* Generate copy of experiment.json */
	i_name = json_find(experiment, "name");
	j_datetime = json_new(JSON_STRING);

	json_set_a(j_datetime, datetime);
	json_set_name(j_datetime, "datetime");
	json_insert(experiment, i_name, j_datetime);

	path_join(exp_path, PATHMAXLEN, experiment_dir, DEFAULT_EXPERIMENT_FILENAME, NULL);

	exp_file = fopen(exp_path, "w");
	if(exp_file == NULL)
		return;

	data = json_write_formatted(experiment);

	fputs(data, exp_file);

	json_free(data);
	fclose(exp_file);
}

static int generate_tsfile_walker(hm_item_t* obj, void* ctx) {
	agent_workload_t* awl = (agent_workload_t*) obj;
	char path[PATHMAXLEN];
	char name[128];

	tsfile_schema_t* schema = awl->wl_schema;
	JSONNODE* schema_node;
	char* schema_str;
	FILE* schema_file;

	/* Create TSF schema */
	snprintf(name, 128, "%s-schema.json", awl->wl_name);
	path_join(path, PATHMAXLEN, experiment_dir, name, NULL);

	schema_file = fopen(path, "w");

	if(schema_file == NULL) {
		load_fprintf(stdout, "Failed to create TSF schema file '%s'\n", path);

		load_error = B_TRUE;
		return HM_WALKER_STOP;
	}

	schema_node = json_tsfile_schema_format(schema);
	schema_str = json_write_formatted(schema_node);
	fputs(schema_str, schema_file);
	json_free(schema_str);

	fclose(schema_file);

	/* Create TSF file */
	snprintf(name, 128, "%s.tsf", awl->wl_name);
	path_join(path, PATHMAXLEN, experiment_dir, name, NULL);

	awl->wl_rqreport_file = tsfile_create(path, schema);

	if(awl->wl_rqreport_file == NULL) {
		load_fprintf(stdout, "Failed to create TSF file '%s'\n", path);

		load_error = B_TRUE;
		return HM_WALKER_STOP;
	}

	load_fprintf(stdout, "Created output TSF file '%s'\n", path);

	return HM_WALKER_CONTINUE;
}

static int generate_output_files(JSONNODE* experiment) {
	char experiment_dir_name[128];
	char experiment_root[PATHMAXLEN];

	snprintf(experiment_dir_name, 128, "%s-%ld-%ld",
				experiment_name, generate_experiment_id(), t_get_pid());

	strcpy(experiment_root, experiment_dir);
	path_join(experiment_dir, PATHMAXLEN, experiment_root, experiment_dir_name, NULL);

	if(mkdir(experiment_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0)
		return LOAD_ERR_CONFIGURE;

	experiment_write(experiment);

	hash_map_walk(&agent_wl_hash_map, generate_tsfile_walker, NULL);
	if(load_error) {
		return LOAD_ERR_CONFIGURE;
	}

	return LOAD_OK;
}

#define CONFIGURE_EXPERIMENT_PARAM(iter, name)													\
	iter = json_find(experiment, name);												\
	if(iter == i_end) {																\
		load_fprintf(stderr, "Missing parameter '%s' in experiment's config\n", name);	\
		return LOAD_ERR_CONFIGURE;													\
	}																				\

static int prepare_experiment(void) {
	JSONNODE_ITERATOR i_end, i_name, i_steps, i_threadpools, i_workloads;

	int err = LOAD_OK;
	char* name;

	i_end = json_end(experiment);

	CONFIGURE_EXPERIMENT_PARAM(i_name, "name");
	CONFIGURE_EXPERIMENT_PARAM(i_steps, "steps");
	CONFIGURE_EXPERIMENT_PARAM(i_workloads, "workloads");

	i_threadpools = json_find(experiment, "threadpools");

	name = json_as_string(*i_name);
	strncpy(experiment_name, name, EXPNAMELEN);
	json_free(name);

	load_fprintf(stdout, "=== CONFIGURING EXPERIMENT '%s' === \n", experiment_name);
	load_fprintf(stdout, "Found %d threadpools\n", (i_threadpools != i_end)
				? json_size(*i_threadpools)
				: 0);
	load_fprintf(stdout, "Found %d workloads\n", json_size(*i_workloads));
	load_fprintf(stdout, "Log path: %s\n", log_filename);
	load_fprintf(stdout, "Requests report path: %s\n", rqreport_filename);

	if(i_threadpools != i_end)
		err = configure_threadpools(*i_threadpools);

	if(err == LOAD_OK)
		err = configure_all_wls(*i_steps, *i_workloads);

	if(err == LOAD_OK)
		err = generate_output_files(experiment);

	return err;
}

void do_error_msg(ts_errcode_t code, const char* format, ...) {
	va_list va;
	char fmtstr[256];

	snprintf(fmtstr, 256, "ERROR %d: %s\n", code, format);

	va_start(va, format);
	load_vfprintf(stderr, fmtstr, va);
	va_end(va);

	load_error = B_TRUE;

	event_notify_all(&workload_event);
}

void do_workload_status(const char* wl_name,
					 			 int status,
								 long progress,
								 const char* config_msg) {
	agent_workload_t* awl = hash_map_find(&agent_wl_hash_map, wl_name);

	assert(awl != NULL);

	if(status == WLS_CFG_FAIL ||
	   status == WLS_CONFIGURED) {
		/* Workload ended it's configuration, may go further */
		atomic_dec(&wl_cfg_count);

		if(status == WLS_CFG_FAIL) {
			/* One of workload is failed to configure */
			load_error = B_TRUE;
		}
	}

	if(status == WLS_RUNNING) {
		/* Feed workload with one step */
		if(load_provide_step(awl) == STEP_ERROR) {
			load_error = B_TRUE;

			/* Stop running experiment by setting wl_run_count to zero */
			atomic_set(&wl_run_count, 0);
		}
	}

	if(status == WLS_FINISHED) {
		atomic_dec(&wl_run_count);
	}

	load_fprintf(stderr, "Workload %s %s (%ld): %s\n", wl_name, wl_status_msg(status), progress, config_msg);

	event_notify_all(&workload_event);
}

static void report_request(request_t* rq) {
	agent_workload_t* awl = hash_map_find(&agent_wl_hash_map,
										  rq->rq_workload->wl_name);

	size_t rqe_size;
	struct request_entry* rqe;

	assert(awl != NULL);

	rqe_size = awl->wl_schema->hdr.entry_size;
	rqe = mp_malloc(rqe_size);

	rqe->step = rq->rq_step;
	rqe->request = rq->rq_id;
	rqe->thread = rq->rq_thread_id;

	rqe->sched_time = rq->rq_sched_time;
	rqe->start_time = rq->rq_start_time;
	rqe->end_time = rq->rq_end_time;

	rqe->flags = rq->rq_flags;

	tsfile_add(awl->wl_rqreport_file, rqe, 1);

	mp_free(rqe);

	if(!awl->wl_is_chained)
		atomic_dec(&rq_count);
}

void do_requests_report(list_head_t* rq_list) {
	request_t *rq_root, *rq;
	int count = 0;

	list_for_each_entry(request_t, rq_root, rq_list, rq_node) {
		rq = rq_root;
		do {
			report_request(rq);

			rq = rq->rq_chain_next;
			++count;
		} while(rq != NULL);
	}

	event_notify_all(&request_event);

	if(count > 0)
		load_fprintf(stdout, "Reported %d requests\n", count);
}

/**
 * Load system with requests
 */
int do_load(void) {
	int err = LOAD_OK;
	workload_t* wl = NULL;

	path_join(experiment_filename, PATHMAXLEN, experiment_dir, DEFAULT_EXPERIMENT_FILENAME, NULL);

	wl_cfg_count = (atomic_t) 0l;
	wl_run_count = (atomic_t) 0l;
	rq_count = (atomic_t) 0l;

	/* Install tsload/tsfile handlers */
	tsload_error_msg = do_error_msg;
	tsfile_error_msg = do_error_msg;
	tsload_workload_status = do_workload_status;
	tsload_requests_report = do_requests_report;

	/* Open and read experiment's configuration */
	if((err = experiment_read()) != LOAD_OK) {
		return err;
	}

	/* Prepare experiment: process configuration and configure workloads */
	if((err = prepare_experiment()) != LOAD_OK) {
		goto end;
	}

	/* Wait until workload configuration is complete*/
	while(atomic_read(&wl_cfg_count) != 0) {
		event_wait(&workload_event);
	}

	if(!load_error && err == LOAD_OK) {
		/* Run experiment */
		start_all_wls();

		/* Wait until all workloads are complete or failure will occur*/
		while(atomic_read(&wl_run_count) != 0) {
			event_wait(&workload_event);
		}
	}

	load_fprintf(stdout, "\n\n=== FINISHING EXPERIMENT '%s' === \n", experiment_name);

	while(atomic_read(&rq_count) != 0) {
		event_wait(&request_event);
	}

end:
	unconfigure_all_wls();
	unconfigure_threadpools();

	json_delete(experiment);

	return err;
}

int load_init(void) {
	hash_map_init(&agent_wl_hash_map, "agent_wl_hash_map");

	event_init(&workload_event, "workload_event");
	event_init(&request_event, "request_event");
	mutex_init(&output_lock, "output_lock");

	return 0;
}

void load_fini(void) {
	event_destroy(&request_event);
	event_destroy(&workload_event);
	mutex_destroy(&output_lock);

	hash_map_destroy(&agent_wl_hash_map);
}
