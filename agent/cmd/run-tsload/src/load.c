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

#define TSLOAD_IMPORT LIBIMPORT
#include <tsload.h>

#include <commands.h>
#include <steps.h>

#include <libjson.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

char experiment_dir[PATHMAXLEN];

char experiment_filename[PATHMAXLEN];
char rqreport_filename[PATHMAXLEN];

char* exp_name;

LIBIMPORT char log_filename[];

JSONNODE* experiment = NULL;

char experiment_name[EXPNAMELEN];

/* Wait until experiment finishes or fails */
thread_mutex_t	output_lock;
thread_mutex_t	rqreport_lock;
thread_event_t	workload_event;

char**			wl_name_array = NULL;	/* Names of configured workloads */
long 			wl_count = 0;
atomic_t		wl_cfg_count;
atomic_t		wl_run_count;

char**			tp_name_array = NULL;	/* Names of configured threadpools */
int				tp_count = 0;

boolean_t		load_error = B_FALSE;

FILE*			rqreport_file;

const char* wl_status_msg(wl_status_t wls) {
	switch(wls) {
	case WLS_CONFIGURING:
		return "configuring";
	case WLS_SUCCESS:
		return "successfully configured";
	case WLS_FAIL:
		return "configuration failed";
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
static int load_steps(const char* wl_name, JSONNODE* steps_node) {
	int ret = LOAD_OK;

	JSONNODE_ITERATOR	i_step, i_end, i_file, i_const_rqs, i_const_steps;
	char* step_fn;
	char step_filename[PATHMAXLEN];

	long num_steps;
	unsigned num_requests;

	i_end = json_end(steps_node);
	i_step = json_find(steps_node, wl_name);

	if( i_step == i_end ||
		json_type(*i_step) != JSON_NODE) {
			load_fprintf(stderr, "Missing steps for workload %s\n", wl_name);
			return LOAD_ERR_CONFIGURE;
	}

	i_end = json_end(*i_step);
	i_file = json_find(*i_step, "file");

	if(i_file != i_end) {
		if(json_type(*i_file) != JSON_STRING) {
			load_fprintf(stderr, "Invalid steps description for workload %s: 'file' should be a string\n", wl_name);
			return LOAD_ERR_CONFIGURE;
		}

		step_fn = json_as_string(*i_file);
		path_join(step_filename, PATHMAXLEN, experiment_dir, step_fn, NULL);

		if(step_create_file(wl_name, step_filename) != STEP_OK) {
			load_fprintf(stderr, "Couldn't open steps file for workload %s\n", step_filename, wl_name);
			ret = LOAD_ERR_CONFIGURE;
		}

		load_fprintf(stderr, "Loaded steps file %s for workload %s\n", step_filename, wl_name);

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
							return LOAD_ERR_CONFIGURE;
			}

			num_steps = json_as_int(*i_const_steps);
			num_requests = json_as_int(*i_const_rqs);

			step_create_const(wl_name, num_steps, num_requests);

			load_fprintf(stderr, "Created constant steps generator for workload %s with N=%d R=%d\n",
					wl_name, num_steps, num_requests);
		}
		else {
			load_fprintf(stderr, "Not found steps parameters for workload %s\n", wl_name);
			return LOAD_ERR_CONFIGURE;
		}
	}

	return ret;
}

/**
 * Read step from steps file than provide it to tsload core
 * If it encounters parse error, set load_error flag
 *
 * @return result of step_get_step*/
int load_provide_step(const char* wl_name) {
	unsigned num_rqs = 0;
	long step_id = -1;
	int status;
	int ret = step_get_step(wl_name, &step_id, &num_rqs);

	if(ret == STEP_ERROR) {
		load_fprintf(stderr, "Cannot process step %ld: cannot parse or internal error!\n", step_id);
		load_error = B_TRUE;

		return ret;
	}

	if(ret == STEP_OK)
		tsload_provide_step(wl_name, step_id, num_rqs, &status);

	return ret;
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

	int num = json_size(wl_node);
	int steps_err;
	int tsload_ret;

	load_fprintf(stdout, "\n\n==== CONFIGURING WORKLOADS ====\n");

	wl_name_array = mp_malloc(num * sizeof(char*));

	if(json_type(steps_node) != JSON_NODE) {
		load_fprintf(stderr, "Cannot process steps: not a JSON node\n");
		return LOAD_ERR_CONFIGURE;
	}

	while(iter != end) {
		wl_name = json_name(*iter);

		steps_err = load_steps(wl_name, steps_node);
		if(steps_err != LOAD_OK) {
			/* Error encountered during configuration - do not configure other workloads */
			return LOAD_ERR_CONFIGURE;
		}

		tsload_configure_workload(wl_name, *iter);

		load_fprintf(stdout, "Initialized workload %s\n", wl_name);
		wl_name_array[wl_count++] = wl_name;

		if(load_error) {
			return LOAD_ERR_CONFIGURE;
		}

		atomic_inc(&wl_cfg_count);

		++iter;
	}

	return LOAD_OK;
}

static void unconfigure_all_wls(void) {
	int i;

	for(i = 0; i < wl_count; ++i) {
		tsload_unconfigure_workload(wl_name_array[i]);
		json_free(wl_name_array[i]);
	}

	mp_free(wl_name_array);
}

/**
 * Feeds workloads with first WLSTEPQSIZE steps and schedules workloads to start
 * Workloads are starting at the same time, but  */
static void start_all_wls(void) {
	int i, j;
	int ret;

	char start_time_str[32];

	ts_time_t start_time = tm_get_time() + WL_START_DELAY;
	time_t unix_start_time = TS_TIME_TO_UNIX(start_time);

	strftime(start_time_str, 32, "%H:%M:%S %d.%m.%Y", localtime(&unix_start_time));

	load_fprintf(stdout, "\n\n=== RUNNING EXPERIMENT '%s' === \n", experiment_name);

	for(i = 0; i < wl_count; ++i) {
		for(j = 0; j < WLSTEPQSIZE; ++j) {
			ret = load_provide_step(wl_name_array[i]);

			if(ret == STEP_ERROR)
				return;

			if(ret == STEP_NO_RQS)
				break;
		}

		tsload_start_workload(wl_name_array[i], start_time);

		load_fprintf(stdout, "Scheduling workload %s at %s,%03ld (provided %d steps)\n", wl_name_array[i],
				start_time_str, TS_TIME_MS(start_time), j);

		atomic_inc(&wl_run_count);

		if(load_error)
			return;
	}
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
	JSONNODE_ITERATOR i_num_threads, i_quantum, i_end, i_disp_name;
	unsigned num_threads;
	ts_time_t quantum;

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
		CONFIGURE_TP_PARAM(i_disp_name, "disp_name", JSON_STRING);

		num_threads = json_as_int(*i_num_threads);
		quantum = json_as_int(*i_quantum);
		disp_name = json_as_string(*i_disp_name);

		tsload_ret = tsload_create_threadpool(tp_name, num_threads, quantum, disp_name);

		if(tsload_ret != TSLOAD_OK) {
			return LOAD_ERR_CONFIGURE;
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

	rqreport_file = fopen(rqreport_filename, "w");

	if(!rqreport_file)
		return LOAD_ERR_CONFIGURE;

	i_end = json_end(experiment);

	CONFIGURE_EXPERIMENT_PARAM(i_name, "name");
	CONFIGURE_EXPERIMENT_PARAM(i_steps, "steps");
	CONFIGURE_EXPERIMENT_PARAM(i_workloads, "workloads");
	CONFIGURE_EXPERIMENT_PARAM(i_threadpools, "threadpools");

	name = json_as_string(*i_name);
	strncpy(experiment_name, name, EXPNAMELEN);
	json_free(name);

	load_fprintf(stdout, "=== CONFIGURING EXPERIMENT '%s' === \n", experiment_name);
	load_fprintf(stdout, "Found %d threadpools\n", json_size(*i_threadpools));
	load_fprintf(stdout, "Found %d workloads\n", json_size(*i_workloads));
	load_fprintf(stdout, "Log path: %s\n", log_filename);
	load_fprintf(stdout, "Requests report path: %s\n", rqreport_filename);

	err = configure_threadpools(*i_threadpools);
	if(err == LOAD_OK)
		err = configure_all_wls(*i_steps, *i_workloads);

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
	if(status == WLS_FAIL ||
	   status == WLS_SUCCESS) {
		/* Workload ended it's configuration, may go further */
		atomic_dec(&wl_cfg_count);

		if(status == WLS_FAIL) {
			/* One of workload is failed to configure */
			load_error = B_TRUE;
		}
	}

	if(status == WLS_RUNNING) {
		/* Feed workload with one step */
		if(load_provide_step(wl_name) == STEP_ERROR) {
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

void do_requests_report(list_head_t* rq_list) {
	request_t* rq;

	mutex_lock(&rqreport_lock);
	list_for_each_entry(request_t, rq, rq_list, rq_node) {
		fprintf(rqreport_file, "%s,%ld,%d,%d,%lld,%lld,%x\n",
				rq->rq_workload->wl_name,
				rq->rq_step,
				rq->rq_id,
				rq->rq_thread_id,
				rq->rq_start_time,
				rq->rq_end_time,
				rq->rq_flags);
	}
	mutex_unlock(&rqreport_lock);
}

void generate_rqreport_filename(void) {
	time_t now;
	int i = 0;

	char rqreport_fn[32];
	char rqreport_filename_temp[PATHMAXLEN];

	/* Generate request-report filename. If file already exists, choose next name with suffix .1 */
	now = time(NULL);

	strftime(rqreport_fn, 32, "rq-%Y-%m-%d-%H_%M_%S.out", localtime(&now));
	path_join(rqreport_filename_temp, PATHMAXLEN, experiment_dir, rqreport_fn, NULL);

	do {
		snprintf(rqreport_filename, PATHMAXLEN, "%s.%d", rqreport_filename_temp, i);

		++i;
	} while(access(rqreport_filename, F_OK) != -1);
}

/**
 * Load system with requests
 */
int do_load() {
	int err = LOAD_OK;
	workload_t* wl = NULL;

	path_join(experiment_filename, PATHMAXLEN, experiment_dir, DEFAULT_EXPERIMENT_FILENAME, NULL);
	generate_rqreport_filename();

	wl_cfg_count = (atomic_t) 0l;
	wl_run_count = (atomic_t) 0l;

	/* Install tsload handlers */
	tsload_error_msg = do_error_msg;
	tsload_workload_status = do_workload_status;
	tsload_requests_report = do_requests_report;

	/* Open and read experiment's configuration */
	if((err = experiment_read()) != LOAD_OK) {
		return err;
	}

	/* Prepare experiment: process configuration and configure workloads */
	err = prepare_experiment();

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

	unconfigure_all_wls();
	unconfigure_threadpools();

	json_delete(experiment);

	return err;
}

int load_init(void) {
	event_init(&workload_event, "workload_event");
	mutex_init(&output_lock, "output_lock");
	mutex_init(&rqreport_lock, "rqreport_lock");

	return 0;
}

void load_fini(void) {
	event_destroy(&workload_event);
	mutex_destroy(&output_lock);
	mutex_destroy(&rqreport_lock);
}
