/*
 * run.c
 *
 *  Created on: Feb 8, 2014
 *      Author: myaut
 */

#define LOG_SOURCE "experiment"
#include <log.h>

#include <threads.h>
#include <getopt.h>
#include <tsfile.h>
#include <tsload.h>
#include <tstime.h>
#include <workload.h>
#include <wltype.h>
#include <mempool.h>

#include <experiment.h>
#include <commands.h>
#include <steps.h>

#include <stdio.h>
#include <stdarg.h>

#include <assert.h>

ts_time_t tse_run_wl_start_delay = 2 * T_SEC;

experiment_t* running = NULL;
static thread_mutex_t	running_lock;

static thread_mutex_t	output_lock;

boolean_t tse_run_batch_mode = B_FALSE;

int tse_experiment_set(experiment_t* exp, const char* option);
int tse_prepare_experiment(experiment_t* exp);

static int tse_run_fprintf(experiment_t* exp, const char* fmt, ...) {
	va_list va;
	int ret;

	mutex_lock(&output_lock);

	if(!tse_run_batch_mode) {
		va_start(va, fmt);
		ret = vfprintf(stderr, fmt, va);
		va_end(va);
	}

	va_start(va, fmt);
	ret = vfprintf(exp->exp_log, fmt, va);
	va_end(va);

	fflush(stderr);
	fflush(exp->exp_log);

	mutex_unlock(&output_lock);

	return ret;
}

static const char* tse_run_wl_status_msg(wl_status_t wls) {
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
	case WLS_DESTROYED:
		return "destroyed";
	}

	return "";
}

/* ------------------------
 * experiment_load_steps()
 * ------------------------
 */

struct exp_load_steps_context {
	int error;
	experiment_t* exp;
};

/**
 * Reads steps file name from configuration for workload wl_name; opens
 * steps file or creates const generator. */
int exp_steps_walk(hm_item_t* item, void* context) {
	struct exp_load_steps_context* ctx = (struct exp_load_steps_context*) context;
	exp_workload_t* ewl = (exp_workload_t*) item;

	JSONNODE_ITERATOR i_end, i_file, i_const_rqs, i_const_steps;
	char* step_fn;
	char step_path[PATHMAXLEN];
	char step_dest_path[PATHMAXLEN];

	long num_steps;
	unsigned num_requests;

	steps_generator_t* sg = NULL;

	if(ewl->wl_is_chained) {
		/* Chained workloads shouldn't have steps */
		return HM_WALKER_CONTINUE;
	}

	if(ewl->wl_steps_cfg == NULL) {
		tse_run_fprintf(ctx->exp, "Missing steps for workload %s\n", ewl->wl_name);
	}

	i_end = json_end(ewl->wl_steps_cfg);
	i_file = json_find(ewl->wl_steps_cfg, "file");

	if(i_file != i_end) {
		step_fn = json_as_string(*i_file);
		path_join(step_path, PATHMAXLEN, ctx->exp->exp_root, ctx->exp->exp_basedir, step_fn, NULL);
		path_join(step_dest_path, PATHMAXLEN, ctx->exp->exp_root, ctx->exp->exp_directory, step_fn, NULL);
		json_free(step_fn);

		sg = step_create_file(step_path, step_dest_path);

		if(sg != NULL) {
			tse_run_fprintf(ctx->exp, "Loaded steps file '%s' for workload '%s'\n",
							step_path, ewl->wl_name);
		}
		else {
			tse_run_fprintf(ctx->exp, "Couldn't open steps file '%s' for workload '%s'\n",
							step_path, ewl->wl_name);
			ctx->error = -1;
			return HM_WALKER_STOP;
		}
	}
	else {
		i_const_rqs = json_find(ewl->wl_steps_cfg, "num_requests");
		i_const_steps = json_find(ewl->wl_steps_cfg, "num_steps");

		if(i_const_rqs != i_end && i_const_steps != i_end) {
			num_steps = json_as_int(*i_const_steps);
			num_requests = json_as_int(*i_const_rqs);

			sg = step_create_const(num_steps, num_requests);

			tse_run_fprintf(ctx->exp, "Created const steps generator for workload '%s' with N=%ld R=%u\n",
						 	ewl->wl_name, num_steps, num_requests);
		}
		else {
			tse_run_fprintf(ctx->exp, "Not found steps parameters for workload '%s'\n", ewl->wl_name);
			ctx->error = -2;
			return HM_WALKER_STOP;
		}
	}

	ewl->wl_steps = sg;

	return HM_WALKER_CONTINUE;
}

int experiment_load_steps(experiment_t* exp) {
	struct exp_load_steps_context ctx;

	ctx.error = 0;
	ctx.exp = exp;

	hash_map_walk(exp->exp_workloads, &exp_steps_walk, &ctx);

	return ctx.error;
}

static void tse_run_report_request(request_t* rq) {
	exp_workload_t* ewl = hash_map_find(running->exp_workloads,
						  			    rq->rq_workload->wl_name);
	exp_request_entry_t* rqe;

	size_t rqparams_size = rq->rq_workload->wl_type->wlt_rqparams_size;

	size_t rqe_size;
	ptrdiff_t rqparams_start;

	rqe_size = ewl->wl_file_schema->hdr.entry_size;

	rqe = mp_malloc(rqe_size);

	rqe->rq_step = rq->rq_step;
	rqe->rq_request = rq->rq_id;
	rqe->rq_thread = rq->rq_thread_id;
	rqe->rq_user = rq->rq_user_id;

	rqe->rq_sched_time = rq->rq_sched_time;
	rqe->rq_start_time = rq->rq_start_time;
	rqe->rq_end_time = rq->rq_end_time;

	rqe->rq_queue_length = rq->rq_queue_len;

	rqe->rq_flags = rq->rq_flags & RQF_FLAG_MASK;

	rqparams_start = sizeof(exp_request_entry_t);

	/* Write raw rqparams */
	assert((rqe_size - rqparams_start) >= rqparams_size);
	memcpy(((char*) rqe) + rqparams_start, rq->rq_params, rqparams_size);

	tsfile_add(ewl->wl_file, rqe, 1);

	mp_free(rqe);
}

void tse_run_requests_report(list_head_t* rq_list) {
	request_t *rq_root, *rq;
	int count = 0;

	list_for_each_entry(request_t, rq_root, rq_list, rq_node) {
		rq = rq_root;
		do {
			tse_run_report_request(rq);

			rq = rq->rq_chain_next;
			++count;
		} while(rq != NULL);
	}

	if(count > 0) {
		/* TODO: Report per-workload statistics */

		tse_run_fprintf(running, "Reported %d requests\n", count);
	}
}

void tse_run_error_handler(ts_errcode_t code, const char* format, ...) {
	va_list va;
	char fmtstr[256];

	snprintf(fmtstr, 256, "ERROR %d: %s\n", code, format);

	va_start(va, format);
	tse_run_fprintf(running, fmtstr, va);
	va_end(va);

	mutex_lock(&running->exp_mutex);
	running->exp_status = EXPERIMENT_ERROR;
	running->exp_error = code;

	mutex_unlock(&running->exp_mutex);
}

/* -----------------------
 * experiment_run()
 * 	+ configuring
 * -----------------------
 */

int tse_run_tp_configure_walk(hm_item_t* item, void* context) {
	exp_threadpool_t* etp = (exp_threadpool_t*) item;
	char quantum[40];

	int ret = tsload_create_threadpool(etp->tp_name, etp->tp_num_threads, etp->tp_quantum,
			                           etp->tp_discard, etp->tp_disp);

	if(ret != TSLOAD_OK) {
		etp->tp_status = EXPERIMENT_ERROR;
		return HM_WALKER_STOP;
	}

	tm_human_print(etp->tp_quantum, quantum, 40);
	tse_run_fprintf(running, "Configured threadpool '%s' with %d threads and %s quantum\n",
					etp->tp_name, etp->tp_num_threads, quantum);

	etp->tp_status = EXPERIMENT_OK;

	if(etp->tp_sched != NULL) {
		ret = tsload_schedule_threadpool(etp->tp_name, etp);

		if(ret != TSLOAD_OK) {
			etp->tp_status = EXPERIMENT_ERROR;
			return HM_WALKER_STOP;
		}
	}

	return HM_WALKER_CONTINUE;
}

int tse_run_tp_unconfigure_walk(hm_item_t* item, void* context) {
	exp_threadpool_t* etp = (exp_threadpool_t*) item;

	if(etp->tp_status != EXPERIMENT_NOT_CONFIGURED) {
		tsload_destroy_threadpool(etp->tp_name);

		tse_run_fprintf(running, "Unconfigured threadpool '%s (status: %d)'\n",
				        etp->tp_name, etp->tp_status);
	}

	return HM_WALKER_CONTINUE;
}

int tse_run_wl_configure_walk(hm_item_t* item, void* context) {
	exp_workload_t* ewl = (exp_workload_t*) item;

	int ret = tsload_configure_workload(ewl->wl_name, ewl->wl_type,
										(ewl->wl_is_chained)? NULL : ewl->wl_tp_name,
										(ewl->wl_is_chained)? ewl->wl_chain_name : NULL,
										ewl->wl_rqsched, ewl->wl_params);

	if(ret != TSLOAD_OK) {
		ewl->wl_status = EXPERIMENT_ERROR;
		return HM_WALKER_STOP;
	}

	mutex_lock(&running->exp_mutex);
	++running->exp_wl_count;
	++running->exp_wl_configuring_count;
	mutex_unlock(&running->exp_mutex);

	ewl->wl_status = EXPERIMENT_OK;

	return HM_WALKER_CONTINUE;
}

int tse_run_wl_provide_step(exp_workload_t* ewl) {
	unsigned num_rqs = 0;
	long step_id = -1;
	int status;
	int ret;

	if(ewl->wl_is_chained)
		return STEP_NO_RQS;

	ret = step_get_step(ewl->wl_steps, &step_id, &num_rqs);

	if(ret == STEP_ERROR) {
		tse_run_fprintf(running,
				        "Cannot process step %ld for workload '%s'\n", step_id, ewl->wl_name);
		ewl->wl_status = EXPERIMENT_ERROR;
		return ret;
	}

	if(ret == STEP_OK)
		tsload_provide_step(ewl->wl_name, step_id, num_rqs, &status);

	return ret;
}

int tse_run_wl_start_walk(hm_item_t* item, void* context) {
	exp_workload_t* ewl = (exp_workload_t*) item;
	ts_time_t start_time = * (ts_time_t*) context;

	int step;
	int ret;

	mutex_lock(&running->exp_mutex);

	for(step = 0; step < WLSTEPQSIZE; ++step) {
		ret = tse_run_wl_provide_step(ewl);

		if(ret == STEP_ERROR) {
			running->exp_status = EXPERIMENT_ERROR;
			mutex_unlock(&running->exp_mutex);
			return HM_WALKER_STOP;
		}

		if(ret == STEP_NO_RQS)
			break;
	}

	tsload_start_workload(ewl->wl_name, start_time);
	++running->exp_wl_running_count;

	mutex_unlock(&running->exp_mutex);

	tse_run_fprintf(running, "Starting workload '%s': provided %d steps\n", ewl->wl_name, step);

	return HM_WALKER_CONTINUE;
}

int tse_run_wl_unconfigure_walk(hm_item_t* item, void* context) {
	exp_workload_t* ewl = (exp_workload_t*) item;

	if(ewl->wl_status != EXPERIMENT_NOT_CONFIGURED) {
		tsload_unconfigure_workload(ewl->wl_name);

		tse_run_fprintf(running, "Unconfigured workload '%s' (status: %d)\n",
						 ewl->wl_name, ewl->wl_status);
	}

	if(ewl->wl_steps) {
		step_destroy(ewl->wl_steps);
	}

	return HM_WALKER_CONTINUE;
}

void tse_run_workload_status(const char* wl_name,
					 			 int status,
								 long progress,
								 const char* config_msg) {
	exp_workload_t* ewl = hash_map_find(running->exp_workloads, wl_name);

	assert(ewl != NULL);

	mutex_lock(&running->exp_mutex);

	switch(status) {
	case WLS_CFG_FAIL:
		running->exp_status = EXPERIMENT_ERROR;
		/*FALLTHROUGH*/
	case WLS_CONFIGURED:
		/* Workload ended it's configuration, may go further */
		--running->exp_wl_configuring_count;
		cv_notify_one(&running->exp_cv);
		break;
	case WLS_RUNNING:
		/* Feed workload with one step */
		if(tse_run_wl_provide_step(ewl) == STEP_ERROR) {
			running->exp_status = EXPERIMENT_ERROR;

			/* Force main thread start unconfiguring workloads */
			running->exp_wl_running_count = 0;
			cv_notify_one(&running->exp_cv);
		}
		break;
	case WLS_FINISHED:
		--running->exp_wl_running_count;
		cv_notify_one(&running->exp_cv);
		break;
	case WLS_DESTROYED:
		--running->exp_wl_count;
		cv_notify_one(&running->exp_cv);
		break;
	}

	mutex_unlock(&running->exp_mutex);

	tse_run_fprintf(running, "Workload '%s' %s (%ld): %s\n", wl_name,
					tse_run_wl_status_msg(status), progress, config_msg);
}

void experiment_run(experiment_t* exp) {
	ts_time_t start_time;
	char start_time_str[32];

	if(exp->exp_runid == EXPERIMENT_ROOT) {
		logmsg(LOG_CRIT, "Couldn't run experiment '%s' - got root experiment config", exp->exp_name);
		return;
	}

	mutex_lock(&running_lock);
	if(running != NULL) {
		logmsg(LOG_CRIT, "Couldn't run experiment '%s' - already running one", exp->exp_name);
		mutex_unlock(&running_lock);
		return;
	}
	running = exp;
	mutex_unlock(&running_lock);

	tse_run_fprintf(exp, "\n=== CONFIGURING EXPERIMENT '%s' RUN #%d === \n", exp->exp_name, exp->exp_runid);

	hash_map_walk(exp->exp_threadpools, tse_run_tp_configure_walk, NULL);
	if(exp->exp_status != EXPERIMENT_OK) {
		tse_run_fprintf(exp, "Failure occured while spawning threadpools\n");
		goto unconfigure;
	}

	hash_map_walk(exp->exp_workloads, tse_run_wl_configure_walk, NULL);
	if(exp->exp_status != EXPERIMENT_OK) {
		tse_run_fprintf(exp, "Failure occured while configuring workloads\n");
		goto unconfigure;
	}

	mutex_lock(&exp->exp_mutex);
	while(exp->exp_wl_configuring_count > 0)
		cv_wait(&exp->exp_cv, &exp->exp_mutex);
	mutex_unlock(&exp->exp_mutex);

	/* Ceil start time up to second */
	start_time = tm_get_time() + (T_SEC / 2);
	start_time = ((ts_time_t) (start_time / T_SEC)) * T_SEC + tse_run_wl_start_delay;

	start_time_str[0] = '\0';
	tm_datetime_print(start_time, start_time_str, 32);

	experiment_cfg_add(exp->exp_config, NULL,
				       json_new_i("start_time", start_time), B_TRUE);

	tse_run_fprintf(exp, "\n=== STARTING WORKLOADS @%s === \n", start_time_str);

	hash_map_walk(exp->exp_workloads, tse_run_wl_start_walk, &start_time);
	if(exp->exp_status != EXPERIMENT_OK) {
		tse_run_fprintf(exp, "Failure occured while scheduling workloads to start\n");
		goto unconfigure;
	}

	mutex_lock(&exp->exp_mutex);
	while(exp->exp_wl_running_count > 0 && exp->exp_status == EXPERIMENT_OK)
		cv_wait(&exp->exp_cv, &exp->exp_mutex);
	mutex_unlock(&exp->exp_mutex);

unconfigure:
	tse_run_fprintf(exp, "\n=== UNCONFIGURING EXPERIMENT '%s' === \n", exp->exp_name);

	if(exp->exp_status != EXPERIMENT_OK) {
		tse_run_fprintf(exp, "WARNING: Error encountered during experiment run\n");
	}

	hash_map_walk(exp->exp_workloads, tse_run_wl_unconfigure_walk, NULL);

	mutex_lock(&exp->exp_mutex);
	while(exp->exp_wl_count > 0)
		cv_wait(&exp->exp_cv, &exp->exp_mutex);
	mutex_unlock(&exp->exp_mutex);

	hash_map_walk(exp->exp_threadpools, tse_run_tp_unconfigure_walk, NULL);

	mutex_lock(&running_lock);
	running = NULL;
	mutex_unlock(&running_lock);
}

int tse_run(experiment_t* root, int argc, char* argv[]) {
	int c;
	int argi;

	int ret = CMD_OK;
	int err;

	char exp_name[EXPNAMELEN];
	boolean_t nflag = B_FALSE;

	experiment_t* base = NULL;
	experiment_t* exp = NULL;

	JSONNODE* hostinfo;

	exp_name[0] = '\0';

	argi = optind;
	while((c = plat_getopt(argc, argv, "bn:s:")) != -1) {
		switch(c) {
		case 'n':
			strncpy(exp_name, optarg, EXPNAMELEN);
			nflag = B_TRUE;
			break;
		case 'b':
			tse_run_batch_mode = B_TRUE;
			break;
		case 's':
			/* There are no experiment config cause we will get it after read runid argument
			 * So we will rescan options vector after we shift experiment config */
			break;
		case '?':
			if(optopt == 'n' || optopt == 's')
				fprintf(stderr, "-%c option requires an argument\n", optopt);
			else
				fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			return CMD_INVALID_OPT;
		}
	}

	base = tse_shift_experiment_run(root, argc, argv);

	exp = experiment_create(root, base, (nflag)? exp_name : NULL);
	if(exp == NULL) {
		fprintf(stderr, "Couldn't create experiment from %d\n",
				(base == NULL) ? EXPERIMENT_ROOT : base->exp_runid);
		ret = CMD_ERROR;
		goto end_base;
	}

	optind = argi;
	while((c = plat_getopt(argc, argv, "bn:s:")) != -1) {
		switch(c) {
		case 'n':
		case 'b':
			break;
		case 's':
			err = tse_experiment_set(exp, optarg);
			if(err != CMD_OK) {
				fprintf(stderr, "Invalid set option: '%s'\n", optarg);
				goto end;
			}
			break;
		case '?':
			/* Shouldn't be here */
			return CMD_INVALID_OPT;
		}
	}

	ret = tse_prepare_experiment(exp);
	if(ret != CMD_OK)
		goto end;

	/* Install tsload/tsfile handlers */
	tsload_error_msg = tse_run_error_handler;
	tsfile_error_msg = tse_run_error_handler;
	tsload_workload_status = tse_run_workload_status;
	tsload_requests_report = tse_run_requests_report;

	if(tse_run_batch_mode) {
		printf("%d\n", exp->exp_runid);
	}

	experiment_run(exp);

	/* Rewrite experiment config with updated information */
	experiment_cfg_add(exp->exp_config, NULL,
			           json_new_i("status", exp->exp_status), B_TRUE);

	hostinfo = tsload_get_hostinfo();
	json_set_name(hostinfo, "agent");
	experiment_cfg_add(exp->exp_config, NULL, hostinfo, B_TRUE);

	err = experiment_write(exp);
	if(err != EXPERIMENT_WRITE_OK) {
		fprintf(stderr, "Couldn't write config of experiment run. Error: %d\n", err);
		ret = CMD_ERROR;
	}

end:
	experiment_destroy(exp);

end_base:
	if(base != NULL)
		experiment_destroy(base);

	return ret;
}

int tse_experiment_set(experiment_t* exp, const char* option) {
	char name[PATHPARTMAXLEN];
	const char* ptr = strchr(option, '=');
	size_t length;

	if(ptr == NULL)
		return CMD_INVALID_OPT;

	length = min(ptr - option, PATHPARTMAXLEN - 1);
	strncpy(name, option, length);
	name[length] = '\0';

	++ptr;

	if(experiment_cfg_set(exp->exp_config, name, ptr) != EXP_CONFIG_OK) {
		return CMD_INVALID_OPT;
	}

	return CMD_OK;
}

int tse_prepare_experiment(experiment_t* exp) {
	int err;

	/* Add agent info and datetime here */

	err = experiment_process_config(exp);
	if(err != EXP_CONFIG_OK) {
		fprintf(stderr, "Couldn't process experiment config. Error: %d\n", err);
		return CMD_ERROR;
	}

	err = experiment_mkdir(exp);
	if(err != EXPERIMENT_MKDIR_OK) {
		fprintf(stderr, "Couldn't create directory for experiment run. Error: %d\n", err);
		return CMD_ERROR;
	}

	err = experiment_write(exp);
	if(err != EXPERIMENT_WRITE_OK) {
		fprintf(stderr, "Couldn't write config of experiment run. Error: %d\n", err);
		return CMD_ERROR;
	}

	err = experiment_open_log(exp);
	if(err != EXPERIMENT_OPENLOG_OK) {
		fprintf(stderr, "Couldn't open logfile for experiment run. Error: %d\n", err);
		return CMD_ERROR;
	}

	err = experiment_load_steps(exp);
	if(err != 0) {
		fprintf(stderr, "Couldn't load experiment steps. Error: %d\n", err);
		return CMD_ERROR;
	}

	err = experiment_open_workloads(exp, EXP_OPEN_CREATE | EXP_OPEN_RQPARAMS);
	if(err != EXP_OPEN_OK) {
		fprintf(stderr, "Couldn't open workload output files. Error: %d\n", err);
		return CMD_ERROR;
	}

	return CMD_OK;
}

int run_init(void) {
	mutex_init(&output_lock, "output_lock");
	mutex_init(&running_lock, "running_lock");

	return 0;
}

void run_fini(void) {
	mutex_destroy(&output_lock);
	mutex_destroy(&running_lock);
}

