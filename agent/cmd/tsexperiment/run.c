
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



#define LOG_SOURCE "tserun"
#include <tsload/log.h>

#include <tsload/defs.h>

#include <tsload/getopt.h>
#include <tsload/time.h>
#include <tsload/mempool.h>
#include <tsload/tuneit.h>
#include <tsload/signal.h>

#include <tsload.h>
#include <tsload/load/workload.h>
#include <tsload/load/wltype.h>

#include <tseerror.h>
#include <experiment.h>
#include <commands.h>
#include <steps.h>

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

ts_time_t tse_run_intr_poll_interval = 1 * T_SEC;

ts_time_t tse_run_wl_start_delay = 1 * T_SEC;

experiment_t* running = NULL;
static thread_mutex_t	running_lock;
boolean_t interrupted = B_FALSE;

int tse_experiment_set(experiment_t* exp, const char* option);
int tse_prepare_experiment(experiment_t* exp, experiment_t* base, experiment_t* root);

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
    case WLS_STOPPED:
        return "stopped";
	}

	return "";
}

void tse_run_intr_handler(int signum) {
    if(interrupted) {
        reset_signal_func(SIGNAL_INTERRUPT);
        reset_signal_func(SIGNAL_TERMINATE);
    }
    
    interrupted = B_TRUE;
}

/* ------------------------
 * experiment_load_steps()
 * ------------------------
 */

struct exp_create_steps_context {
	unsigned error;
	experiment_t* exp;
	experiment_t* base;
};

/**
 * Reads steps file name from configuration for workload wl_name; opens
 * steps file or creates const generator. */
int exp_create_steps_walk(hm_item_t* item, void* context) {
	struct exp_create_steps_context* ctx = (struct exp_create_steps_context*) context;
	exp_workload_t* ewl = (exp_workload_t*) item;

	json_node_t* j_series = NULL;

	const char* step_fn = NULL;
	
	long num_steps;
	unsigned num_requests;

	steps_generator_t* sg = NULL;

	int error;

	if(ewl->wl_is_chained) {
		/* Chained workloads shouldn't have steps */
		return HM_WALKER_CONTINUE;
	}

	if(ewl->wl_steps_cfg == NULL) {
		tse_experiment_error_msg(ctx->exp, EXPERR_STEPS_INVALID_CONST,
							  "Missing steps for workload %s\n", ewl->wl_name);
		ctx->error = EXPERR_STEPS_INVALID_CONST;
		return HM_WALKER_STOP;
	}

	error = json_get_string(ewl->wl_steps_cfg, "file", &step_fn);
	if(error == JSON_NOT_FOUND) {
		error = json_get_array(ewl->wl_steps_cfg, "series", &j_series);
	}
	
	if(error != JSON_NOT_FOUND) {
		if(error == JSON_INVALID_TYPE) {
			tse_experiment_error_msg(ctx->exp, EXPERR_STEPS_INVALID_FILE,
					"Error parsing step parameters for workload '%s': %s\n",
					ewl->wl_name, json_error_message());
			ctx->error = EXPERR_STEPS_INVALID_FILE;
			return HM_WALKER_STOP;
		}
		
		if (step_fn != NULL) {
			char step_path[PATHMAXLEN];
			path_join(step_path, PATHMAXLEN, ctx->exp->exp_root, ctx->exp->exp_basedir, step_fn, NULL);
			
			char step_dest_path[PATHMAXLEN];
			path_join(step_dest_path, PATHMAXLEN, ctx->exp->exp_root, ctx->exp->exp_directory, step_fn, NULL);

			sg = step_create_file(step_dest_path, step_path, NULL);
			
			if(sg != NULL) {
				tse_printf(TSE_PRINT_ALL, "Loaded steps file '%s' for workload '%s'\n",
								step_path, ewl->wl_name);
			} else {
				tse_experiment_error_msg(ctx->exp, ctx->error = EXPERR_STEPS_FILE_ERROR,
						"Couldn't open steps file '%s' for workload '%s'\n",
						step_path, ewl->wl_name);
				return HM_WALKER_STOP;
			}
		} else if(j_series != NULL) {
			char* step_dest_fn;
			char step_dest_path[PATHMAXLEN];
			
			aas_printf(aas_init(&step_dest_fn), "%s.tss", ewl->wl_name);
			path_join(step_dest_path, PATHMAXLEN, ctx->exp->exp_root, ctx->exp->exp_directory, step_dest_fn, NULL);
			
			sg = step_create_file(step_dest_path, NULL, j_series);
			
			aas_free(&step_dest_fn);
			
			if(sg == NULL) {
				tse_experiment_error_msg(ctx->exp, ctx->error = EXPERR_STEPS_SERIES_ERROR,
						"Couldn't use provided series for workload '%s'\n", ewl->wl_name);
				return HM_WALKER_STOP;
			}
		}
	}
	else {
		int error1 = json_get_integer_u(ewl->wl_steps_cfg, "num_requests", &num_requests);
		int error2 = json_get_integer_l(ewl->wl_steps_cfg, "num_steps", &num_steps);

		if(error1 == JSON_OK && error2 == JSON_OK) {
			sg = step_create_const(num_steps, num_requests);

			tse_printf(TSE_PRINT_ALL,
					   "Created const steps generator for workload '%s' with N=%ld R=%u\n",
						ewl->wl_name, num_steps, num_requests);
		}
		else {
			tse_experiment_error_msg(ctx->exp, ctx->error = EXPERR_STEPS_INVALID_CONST,
							"Error parsing step parameters for workload '%s': %s\n",
							ewl->wl_name, json_error_message());
			return HM_WALKER_STOP;
		}
	}

	if(ctx->exp->exp_trace_mode) {
		ewl->wl_steps = step_create_trace(sg, ctx->base, ewl);

		if(ewl->wl_steps == NULL) {
			step_destroy(sg);

			tse_experiment_error_msg(ctx->exp, ctx->error = EXPERR_STEPS_INVALID_TRACE,
					"Couldn't create trace-driven step generator for workload '%s'\n",
					 ewl->wl_name);
			return HM_WALKER_STOP;
		}
	}
	else {
		ewl->wl_steps = sg;
	}

	return HM_WALKER_CONTINUE;
}

int experiment_create_steps(experiment_t* exp, experiment_t* base) {
	struct exp_create_steps_context ctx;

	ctx.error = 0;
	ctx.exp = exp;
	ctx.base = base;

	hash_map_walk(exp->exp_workloads, &exp_create_steps_walk, &ctx);

	return ctx.error;
}

static void tse_run_report_request(request_t* rq) {
	exp_workload_t* ewl = hash_map_find(running->exp_workloads,
						  			    rq->rq_workload->wl_name);
	exp_request_entry_t* rqe;

	size_t rqparams_size = rq->rq_workload->wl_type->wlt_rqparams_size;
	ts_time_t adjtime = wl_get_clock_adjustement(rq->rq_workload);

	size_t rqe_size;
	ptrdiff_t rqparams_start;

	assert(ewl != NULL);

	rqe_size = ewl->wl_file_schema->hdr.entry_size;

	rqe = mp_malloc(rqe_size);

	rqe->rq_step = rq->rq_step;
	rqe->rq_request = rq->rq_id;
	rqe->rq_thread = rq->rq_thread_id;
	rqe->rq_user = rq->rq_user_id;

	rqe->rq_sched_time = rq->rq_sched_time + adjtime;
	rqe->rq_start_time = rq->rq_start_time + adjtime;
	rqe->rq_end_time = rq->rq_end_time + adjtime;

	rqe->rq_queue_length = rq->rq_queue_len;

	rqe->rq_flags = rq->rq_flags & RQF_FLAG_MASK;

	if(rq->rq_chain_next != NULL) {
		rqe->rq_chain_request = rq->rq_chain_next->rq_id;
	}
	else {
		rqe->rq_chain_request = -1;
	}

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
		tse_printf(TSE_PRINT_NOLOG, "Reported %d requests\n", count);
	}
}

/* -----------------------
 * experiment_run()
 * 	+ configuring
 * -----------------------
 */

int tse_run_tp_configure_walk(hm_item_t* item, void* context) {
	exp_threadpool_t* etp = (exp_threadpool_t*) item;
	experiment_t* exp = (experiment_t*) context;
	char quantum[40];

	int ret;

	json_node_t* tp_disp = etp->tp_disp;
	json_node_t* trace_tp_disp = NULL;

	if(exp->exp_trace_mode) {
		experiment_cfg_add(etp->tp_disp, NULL, JSON_STR("type"),
						   json_new_string(JSON_STR("trace")), B_TRUE);
	}

	ret = tsload_create_threadpool(etp->tp_name, etp->tp_num_threads, etp->tp_quantum,
			                           etp->tp_discard, etp->tp_disp);

	if(ret != TSLOAD_OK) {
		etp->tp_status = EXPERIMENT_ERROR;
		return HM_WALKER_STOP;
	}

	tm_human_print(etp->tp_quantum, quantum, 40);
	tse_printf(TSE_PRINT_NOLOG,
			   "Configured threadpool '%s' with %d threads and %s quantum\n",
			   etp->tp_name, etp->tp_num_threads, quantum);

	etp->tp_status = EXPERIMENT_OK;

	if(etp->tp_sched != NULL) {
		ret = tsload_schedule_threadpool(etp->tp_name, etp->tp_sched);

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

		tse_printf(TSE_PRINT_NOLOG,
				   "Unconfigured threadpool '%s (status: %d)'\n",
				   etp->tp_name, etp->tp_status);
	}

	return HM_WALKER_CONTINUE;
}

int tse_run_wl_configure_walk(hm_item_t* item, void* context) {
	experiment_t* exp = (experiment_t*) context;
	exp_workload_t* ewl = (exp_workload_t*) item;
	workload_template_t wl_tpl;
	int ret;
	
	wl_tpl.wl_type = ewl->wl_type;
	wl_tpl.tp_name = (ewl->wl_is_chained)? NULL : ewl->wl_tp_name;
	wl_tpl.deadline = ewl->wl_deadline;
	wl_tpl.global_time = exp->exp_global_time;
	
	wl_tpl.wl_chain_params = ewl->wl_chain;
	wl_tpl.rqsched_params = ewl->wl_rqsched;
	wl_tpl.wl_params = ewl->wl_params;
	
	ret = tsload_configure_workload(ewl->wl_name, &wl_tpl);
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
	int ret, err;

	list_head_t trace_rqs;

	if(ewl->wl_is_chained)
		return STEP_NO_RQS;

	if(ewl->wl_status == EXPERIMENT_NOT_CONFIGURED)
		return STEP_ERROR;

	list_head_init(&trace_rqs, "trace-rqs-%s", ewl->wl_name);
	ret = step_get_step(ewl->wl_steps, &step_id, &num_rqs, &trace_rqs);

	if(ret == STEP_ERROR) {
		mutex_unlock(&running->exp_mutex); 
		tse_experiment_error_msg(running, EXPERR_STEP_PROVIDE_STEP_ERROR,
				   "Couldn't process step %ld for workload '%s': "
				   "steps failure\n", step_id, ewl->wl_name);
		ewl->wl_status = EXPERIMENT_ERROR;
		return ret;
	}

	/* Do not check if we overflow queue because we provide steps
	 * synchronously to TSLoad Core. But this code relies on logic of wl_provide_step
	 * so still have to be careful. */

	if(ret == STEP_OK) {
		err = tsload_provide_step(ewl->wl_name, step_id, num_rqs, &trace_rqs, &status);

		if(err != TSLOAD_OK) {
			mutex_unlock(&running->exp_mutex);
			tse_experiment_error_msg(running, EXPERR_STEP_PROVIDE_TSLOAD_ERROR,
					   "Couldn't provide step %ld for workload '%s': "
					   "TSLoad core error\n", step_id, ewl->wl_name);

			ewl->wl_status = EXPERIMENT_ERROR;
			return STEP_ERROR;
		}

		if(status == WL_STEP_QUEUE_FULL) {
			mutex_unlock(&running->exp_mutex);
			tse_experiment_error_msg(running, EXPERR_STEP_PROVIDE_QFULL,
					   "Couldn't provide step %ld for workload '%s': "
					   "queue is full\n", step_id, ewl->wl_name);

			ewl->wl_status = EXPERIMENT_ERROR;
			return STEP_ERROR;
		}
	}

	return ret;
}

int tse_run_wl_start_walk(hm_item_t* item, void* context) {
	exp_workload_t* ewl = (exp_workload_t*) item;
	ts_time_t start_time = * (ts_time_t*) context;

	int step;
	int ret;

	mutex_lock(&running->exp_mutex);

	for(step = 0; step < (WLSTEPQSIZE - 1); ++step) {
		ret = tse_run_wl_provide_step(ewl);
		
		/* In case of error, tse_run_wl_provide_step will unlock `exp_mutex`,
		 * no need to unlock it here. */
		
		if(ret == STEP_ERROR) 
			return HM_WALKER_STOP;
		if(ret == STEP_NO_RQS)
			break;
	}

	tsload_start_workload(ewl->wl_name, start_time);
	++running->exp_wl_running_count;

	mutex_unlock(&running->exp_mutex);

	tse_printf(TSE_PRINT_NOLOG,
			   "Starting workload '%s': provided %d steps\n", ewl->wl_name, step);

	return HM_WALKER_CONTINUE;
}

int tse_run_wl_unconfigure_walk(hm_item_t* item, void* context) {
	exp_workload_t* ewl = (exp_workload_t*) item;

	if(ewl->wl_status != EXPERIMENT_NOT_CONFIGURED) {
		tsload_unconfigure_workload(ewl->wl_name);

		tse_printf(TSE_PRINT_NOLOG,
				"Unconfigured workload '%s' (status: %d)\n",
				ewl->wl_name, ewl->wl_status);
	}

	if(ewl->wl_steps) {
		step_destroy(ewl->wl_steps);
	}

	ewl->wl_status = EXPERIMENT_NOT_CONFIGURED;

	return HM_WALKER_CONTINUE;
}

int tse_run_wl_stop_walk(hm_item_t* item, void* context) {
    exp_workload_t* ewl = (exp_workload_t*) item;

    tsload_stop_workload(ewl->wl_name);
    ewl->wl_status = EXPERIMENT_INTERRUPTED;

    return HM_WALKER_CONTINUE;
}

void tse_run_workload_status(const char* wl_name,
					 			 int status,
								 long progress,
								 const char* config_msg) {
	exp_workload_t* ewl = hash_map_find(running->exp_workloads, wl_name);

	assert(ewl != NULL);

	tse_printf(TSE_PRINT_NOLOG,
			"Workload '%s' %s (%ld): %s\n", wl_name,
			tse_run_wl_status_msg(status), progress, config_msg);

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
}

static void experiment_notify_start(experiment_t* exp, ts_time_t start_time) {
	char start_time_str[32];

	start_time_str[0] = '\0';
	tm_datetime_print(start_time, start_time_str, 32);

	experiment_cfg_add(exp->exp_config, NULL, JSON_STR("start_time"),
				       json_new_integer(start_time), B_TRUE);

	tse_printf(TSE_PRINT_NOLOG, "\n=== STARTING WORKLOADS @%s === \n",
			start_time_str);

}

int experiment_configure(experiment_t* exp) {
	tse_printf(TSE_PRINT_NOLOG, "\n=== CONFIGURING EXPERIMENT '%s' RUN #%d === \n",
			exp->exp_name, exp->exp_runid);

	hash_map_walk(exp->exp_threadpools, tse_run_tp_configure_walk, exp);
	if(exp->exp_status != EXPERIMENT_NOT_CONFIGURED) {
		tse_experiment_error_msg(exp, EXPERR_RUN_TP_ERROR,
				"Couldn't run experiment '%s': "
				"Failure occurred while spawning threadpools\n", exp->exp_name);
		return EXPERR_RUN_TP_ERROR;
	}

	hash_map_walk(exp->exp_workloads, tse_run_wl_configure_walk, exp);

	/* Wait until all workloads will configure then revert state if needed
	 * (since configuration process is not interruptible, we have to wait) */
	mutex_lock(&exp->exp_mutex);
	while(exp->exp_wl_configuring_count > 0)
		cv_wait(&exp->exp_cv, &exp->exp_mutex);
	mutex_unlock(&exp->exp_mutex);

	if(exp->exp_status != EXPERIMENT_NOT_CONFIGURED) {
		tse_experiment_error_msg(exp, EXPERR_RUN_WL_ERROR,
				"Couldn't run experiment '%s': "
				"Failure occurred while configuring workloads\n", exp->exp_name);
		return EXPERR_RUN_WL_ERROR;
	}

	return EXPERR_RUN_OK;
}

void experiment_unconfigure(experiment_t* exp) {
	tse_printf(TSE_PRINT_NOLOG, "\n=== UNCONFIGURING EXPERIMENT '%s' RUN #%d === \n",
			exp->exp_name, exp->exp_runid);

	if(exp->exp_status != EXPERIMENT_OK) {
		tse_printf(TSE_PRINT_ALL,
			"WARNING: Error encountered during experiment run\n");
	}

	hash_map_walk(exp->exp_workloads, tse_run_wl_unconfigure_walk, NULL);

	mutex_lock(&exp->exp_mutex);
	while(exp->exp_wl_count > 0)
		cv_wait(&exp->exp_cv, &exp->exp_mutex);
	mutex_unlock(&exp->exp_mutex);

	hash_map_walk(exp->exp_threadpools, tse_run_tp_unconfigure_walk, NULL);
}

int experiment_try_enter(experiment_t* exp) {
	/* Occupy running slot */
	mutex_lock(&running_lock);
	if(running != NULL) {
		tse_experiment_error_msg(exp, EXPERR_RUN_ALREADY_RUNNING,
				"Couldn't run experiment '%s': "
				"already running one", exp->exp_name);
		mutex_unlock(&running_lock);
		return EXPERR_RUN_ALREADY_RUNNING;
	}

	running = exp;
	tse_error_set_experiment(running);
	mutex_unlock(&running_lock);

	return EXPERR_RUN_OK;
}

void experiment_leave(experiment_t* exp) {
	mutex_lock(&running_lock);
	running = NULL;
	mutex_unlock(&running_lock);
}

int experiment_start(experiment_t* exp) {
	ts_time_t start_time;

	/* Ceil start time up to second */
	start_time = tm_get_time() + (T_SEC / 2);
	start_time = ((ts_time_t) (start_time / T_SEC)) * T_SEC + tse_run_wl_start_delay;

	exp->exp_status = EXPERIMENT_OK;

	hash_map_walk(exp->exp_workloads, tse_run_wl_start_walk, &start_time);
	if(exp->exp_status != EXPERIMENT_OK) {
		tse_experiment_error_msg(exp, EXPERR_RUN_SCHED_ERROR,
				"Couldn't run experiment '%s': "
				"Failure occured while scheduling workloads to start\n", exp->exp_name);
		return EXPERR_RUN_SCHED_ERROR;
	}

	experiment_notify_start(exp, start_time);

	return EXPERR_RUN_OK;
}

void experiment_wait(experiment_t* exp) {
	mutex_lock(&exp->exp_mutex);
	while(exp->exp_wl_running_count > 0 && exp->exp_status == EXPERIMENT_OK) {
		cv_wait_timed(&exp->exp_cv, &exp->exp_mutex, tse_run_intr_poll_interval);
        
        if(interrupted) {
            tse_printf(TSE_PRINT_ALL,
                       "INTERRUPT: Experiment run is interrupted\n");
            
            hash_map_walk(exp->exp_workloads, tse_run_wl_stop_walk, NULL);
            interrupted = B_FALSE;
        }
    }
	mutex_unlock(&exp->exp_mutex);
}

int experiment_run(experiment_t* exp) {
	int ret = EXPERR_RUN_OK;

	if(exp->exp_runid == EXPERIMENT_ROOT) {
		tse_experiment_error_msg(exp, EXPERR_RUN_IS_ROOT,
				"Couldn't run experiment '%s': "
				"got root experiment config", exp->exp_name);
		return EXPERR_RUN_IS_ROOT;
	}

	ret = experiment_try_enter(exp);
	if(ret != EXPERR_RUN_OK)
		return ret;

	ret = experiment_configure(exp);
	if(ret != EXPERR_RUN_OK)
		goto unconfigure;

	ret = experiment_start(exp);
	if(ret != EXPERR_RUN_OK)
		goto unconfigure;

	experiment_wait(exp);
unconfigure:
	experiment_unconfigure(exp);

	experiment_leave(exp);

	if(exp->exp_status == EXPERIMENT_OK) {
		exp->exp_status = EXPERIMENT_FINISHED;
	}

	return ret;
}

int tse_run(experiment_t* root, int argc, char* argv[]) {
	int c;
	int argi;

	int ret = CMD_OK;
	int err;

	char* exp_name = NULL;
	boolean_t trace_mode = B_FALSE;
	boolean_t batch_mode = B_FALSE;

	experiment_t* base = NULL;
	experiment_t* exp = NULL;

	json_node_t* hostinfo;

	argi = optind;
	while((c = plat_getopt(argc, argv, "bTn:s:")) != -1) {
		switch(c) {
		case 'n':
			aas_copy(&exp_name, optarg);
			break;
		case 'b':
			batch_mode = B_TRUE;
			tse_error_disable_dest(TSE_ERR_DEST_STDERR);
			break;
		case 'T':
			trace_mode = B_TRUE;
			break;
		case 's':
			/* There are no experiment config cause we will get it after read runid argument
			 * So we will rescan options vector after we shift experiment config */
			break;
		case '?':
			if(optopt == 'n' || optopt == 's')
				tse_command_error_msg(CMD_INVALID_OPT, "-%c option requires an argument\n", optopt);
			else
				tse_command_error_msg(CMD_INVALID_OPT, "Unknown option `-%c'.\n", optopt);
			ret = CMD_INVALID_OPT;
			goto end_name;
		}
	}

	ret = tse_shift_experiment_run(root, &base, argc, argv);
	if(ret != CMD_OK)
		goto end_name;

	exp = experiment_create(root, base, exp_name);
	if(exp == NULL) {
		char* runid_str = AAS_CONST_STR("<root>");
		if(base != NULL && base->exp_runid != EXPERIMENT_ROOT) {
			aas_printf(&runid_str, "%d", base->exp_runid);
		}
		
		ret = CMD_GENERIC_ERROR;
		tse_command_error_msg(CMD_GENERIC_ERROR,
				"Couldn't create experiment from %s\n", runid_str);
		aas_free(&runid_str);
		goto end_base;
	}

	exp->exp_trace_mode = trace_mode;

	optind = argi;
	while((c = plat_getopt(argc, argv, "bTn:s:")) != -1) {
		switch(c) {
		case 'n':
		case 'b':
		case 'T':
			break;
		case 's':
			if(!exp->exp_trace_mode) {
				ret = tse_experiment_set(exp, optarg);
				if(ret != CMD_OK) {
					tse_command_error_msg(ret, "Invalid set option: '%s'\n", optarg);
					goto end;
				}
			}
			else {
				ret = CMD_INVALID_OPT;
				tse_command_error_msg(CMD_INVALID_OPT,
						"Altering experiment options is not allowed in trace mode\n");
				goto end;
			}
			break;
		case '?':
			/* Shouldn't be here */
			ret =  CMD_INVALID_OPT;
			goto end;
		}
	}

	ret = tse_prepare_experiment(exp, base, root);
	if(ret != CMD_OK)
		goto end;

	/* Install tsload/tsfile handlers */
	tsload_register_requests_report_func(tse_run_requests_report);
	tsload_register_workload_status_func(tse_run_workload_status);

	if(batch_mode) {
		printf("%d\n", exp->exp_runid);
	}
	
	/* Setup interrupt handler */
    register_signal_func(SIGNAL_INTERRUPT, tse_run_intr_handler);
    register_signal_func(SIGNAL_TERMINATE, tse_run_intr_handler);

	experiment_run(exp);
	if(exp->exp_status == EXPERIMENT_ERROR)
		ret = CMD_GENERIC_ERROR;

	/* Rewrite experiment config with updated information */
	experiment_cfg_add(exp->exp_config, NULL, JSON_STR("status"),
			           json_new_integer(exp->exp_status), B_TRUE);

	hostinfo = tsload_get_hostinfo();
	experiment_cfg_add(exp->exp_config, NULL, JSON_STR("agent"), hostinfo, B_TRUE);
    
    reset_signal_func(SIGNAL_INTERRUPT);
    reset_signal_func(SIGNAL_TERMINATE);
    
	err = experiment_write(exp);
	if(err != EXPERIMENT_OK) {
		ret = tse_experr_to_cmderr(err);
		tse_command_error_msg(ret, "Couldn't write config of experiment run. Error: %d\n", err);
	}

end:
	experiment_destroy(exp);

end_base:
	if(base != root)
		experiment_destroy(base);

end_name:
	aas_free(&exp_name);

	return ret;
}

int tse_experiment_set(experiment_t* exp, const char* option) {
	char name[PATHPARTMAXLEN];
	const char* ptr = strchr(option, '=');
	size_t length;

	if(ptr == NULL)
		return CMD_INVALID_ARG;

	length = min(ptr - option, PATHPARTMAXLEN - 1);
	strncpy(name, option, length);
	name[length] = '\0';

	++ptr;

	if(experiment_cfg_set(exp->exp_config, name, ptr) != EXP_CONFIG_OK) {
		return CMD_INVALID_ARG;
	}

	return CMD_OK;
}

int tse_prepare_experiment(experiment_t* exp, experiment_t* base, experiment_t* root) {
	int err;
	int ret;

	/* Add agent info and datetime here */

	err = experiment_process_config(exp);
	if(err != EXPERIMENT_OK) {
		ret = tse_experr_to_cmderr(err);
		tse_command_error_msg(ret, "Couldn't process experiment config. Error: %x\n", err);
		return ret;
	}

	err = experiment_mkdir(exp, root);
	if(err != EXPERIMENT_OK) {
		ret = tse_experr_to_cmderr(err);
		tse_command_error_msg(ret, "Couldn't create directory for experiment run. Error: %x\n", err);
		return ret;
	}

	err = experiment_write(exp);
	if(err != EXPERIMENT_OK) {
		ret = tse_experr_to_cmderr(err);
		tse_command_error_msg(ret, "Couldn't write config of experiment run. Error: %x\n", err);
		return ret;
	}

	err = experiment_open_log(exp);
	if(err != EXPERIMENT_OK) {
		ret = tse_experr_to_cmderr(err);
		tse_command_error_msg(ret, "Couldn't open logfile for experiment run. Error: %x\n", err);
		return ret;
	}

	if(exp->exp_trace_mode) {
		if(base->exp_runid != EXPERIMENT_ROOT && base->exp_status != EXPERIMENT_FINISHED) {
			ret = tse_experr_to_cmderr(err);
			tse_command_error_msg(ret, "Couldn't create trace-driven simulation based on unfinished run.\n");
			return ret;
		}

		err = experiment_process_config(base);
		if(err != EXPERIMENT_OK) {
			ret = tse_experr_to_cmderr(err);
			tse_command_error_msg(ret, "Couldn't process base experiment config. Error: %x\n", err);
			return ret;
		}

		err = experiment_open_workloads(base, EXP_OPEN_RQPARAMS);
		if(err != EXPERIMENT_OK) {
			ret = tse_experr_to_cmderr(err);
			tse_command_error_msg(ret, "Couldn't read trace files. Error: %x\n", err);
			return ret;
		}
	}

	err = experiment_create_steps(exp, base);
	if(err != 0) {
		ret = tse_experr_to_cmderr(err);
		tse_command_error_msg(ret, "Couldn't create experiment steps. Error: %x\n", err);
		return ret;
	}

	err = experiment_open_workloads(exp, EXP_OPEN_CREATE | EXP_OPEN_RQPARAMS);
	if(err != EXPERIMENT_OK) {
		ret = tse_experr_to_cmderr(err);
		tse_command_error_msg(ret, "Couldn't open workload output files. Error: %x\n", err);
		return ret;
	}

	return CMD_OK;
}

int run_init(void) {
	mutex_init(&running_lock, "running_lock");

	tuneit_set_int(ts_time_t, tse_run_wl_start_delay);

	return 0;
}

void run_fini(void) {
	mutex_destroy(&running_lock);
}


