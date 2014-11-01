
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



#include <hashmap.h>
#include <mempool.h>
#include <tsfile.h>
#include <workload.h>
#include <tstime.h>
#include <getopt.h>
#include <list.h>

#include <experiment.h>
#include <commands.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

typedef void (*tse_report_wl_func)(experiment_t* exp, exp_workload_t* ewl, void* context);

struct report_walk_context {
	experiment_t* 	   exp;
	tse_report_wl_func func;
	void* 			   context;
};

struct report_time_stats {
	double mean;
	double stddev;
};

struct report_stats {
	char step_name[8];
	int start_idx;
	int start_rq_idx;

	int ontime;
	int onstep;
	int discard;

	struct report_time_stats wait;
	struct report_time_stats exec;
};

int tse_report_workload_walk(hm_item_t* obj, void* context);

int tse_report_common(experiment_t* root, int argc, char* argv[],
					  int flags, tse_report_wl_func report, void* context) {
	experiment_t* exp;
	exp_workload_t* ewl;

	struct report_walk_context ctx;

	int argi;
	int ret = CMD_OK;
	int err;

	exp = tse_shift_experiment_run(root, argc, argv);
	if(exp == NULL) {
		fputs("Couldn't open experiment run\n", stderr);
		return CMD_INVALID_ARG;
	}

	argi = optind;

	err = experiment_process_config(exp);
	if(err != EXP_CONFIG_OK) {
		fprintf(stderr, "Couldn't process experiment run config: %d\n", err);
		ret = CMD_ERROR;
		goto end;
	}

	err = experiment_open_workloads(exp, flags);
	if(err != EXP_OPEN_OK) {
		fprintf(stderr, "Couldn't load workload results: %d\n", err);
		ret = CMD_ERROR;
		goto end;
	}

	if(argi == argc) {
		ctx.context = context;
		ctx.func = report;
		ctx.exp = exp;

		hash_map_walk(exp->exp_workloads, tse_report_workload_walk, &ctx);
	}
	else {
		for( ; argi < argc; ++argi) {
			ewl = hash_map_find(exp->exp_workloads, argv[argi]);
			if(ewl)
				report(exp, ewl, context);
		}
	}

end:
	experiment_destroy(exp);

	return ret;
}

int tse_report_workload_walk(hm_item_t* obj, void* context) {
	struct report_walk_context* ctx = (struct report_walk_context*) context;

	ctx->func(ctx->exp, (exp_workload_t*) obj, ctx->context);

	return HM_WALKER_CONTINUE;
}

void tse_report_statistics(double* times, int from, int to, struct report_time_stats* stats) {
	int idx;
	double mean = 0.0;
	double var = 0.0;

	for(idx = from; idx < to; ++idx) {
		mean += times[idx];
	}
	mean = mean / (double) (to - from);

	for(idx = from; idx < to; ++idx) {
		var += pow(times[idx] - mean, 2);
	}
	var = var / (double) (to - from);

	stats->mean = mean;
	stats->stddev = sqrt(var);
}

void tse_report_step(struct report_stats* stats, double* wait_times, double* exec_times, int idx, int rq_idx) {
	/* Report per-step statistics */
	tse_report_statistics(wait_times, stats->start_idx, idx, &stats->wait);
	tse_report_statistics(exec_times, stats->start_idx, idx, &stats->exec);

	printf("%-8s %-6d %-6d %-6d %-8d %-10.3f %-10.3f %-10.3f %-10.3f\n",
			stats->step_name, rq_idx - stats->start_rq_idx, stats->ontime, stats->onstep, stats->discard,
			stats->wait.mean, stats->wait.stddev, stats->exec.mean, stats->exec.stddev);
}

void tse_process_rq_time(exp_request_entry_t* rqe, double* wait_times, double* exec_times, int idx) {
	double sched_time = ((double) rqe->rq_sched_time) / ((double) T_MS);
	double start_time = ((double) rqe->rq_start_time) / ((double) T_MS);
	double end_time = ((double) rqe->rq_end_time) / ((double) T_MS);

	exec_times[idx] = end_time - start_time;
	wait_times[idx] = start_time - sched_time;
}

void tse_process_rq_flags(exp_request_entry_t* rqe, struct report_stats* stats, long step) {
	if(!(rqe->rq_flags & RQF_FINISHED)) {
		++stats->discard;
	}

	if(rqe->rq_flags & RQF_ONTIME) {
		++stats->ontime;
	}

	if(rqe->rq_step == step) {
		++stats->onstep;
	}
}

void tse_report_workload(experiment_t* exp, exp_workload_t* ewl, void* context) {
	void* entry = mp_malloc(ewl->wl_file_schema->hdr.entry_size);
	uint32_t rq_count = tsfile_get_count(ewl->wl_file);
	exp_request_entry_t* rqe = (exp_request_entry_t*) entry;

	long step = -1;

	int idx = 0;
	int rq_idx;

	struct report_stats total_stats;
	struct report_stats step_stats;

	/* It costs 1 MB of memory per 32k requests.
	 * Still have to be careful with this allocation */
	double* wait_times = mp_malloc(rq_count * sizeof(double));
	double* exec_times = mp_malloc(rq_count * sizeof(double));

	printf("%s\n", ewl->wl_name);
	printf("%-8s %-6s %-6s %-6s %-8s %-21s %-21s\n", "STEP", "COUNT",
				"ONTIME", "ONSTEP", "DISCARD", "WAIT TIME (ms)", "SERVICE TIME (ms)");
	printf("%-8s %-6s %-6s %-6s %-8s %-10s %-10s %-10s %-10s\n", "", "",
				"", "", "", "MEAN", "STDDEV", "MEAN", "STDDEV");

	memset(&total_stats, 0, sizeof(struct report_stats));
	strcpy(total_stats.step_name, "total");

	for(rq_idx = 0; rq_idx < rq_count ; ++rq_idx) {
		tsfile_get_entries(ewl->wl_file, entry, rq_idx, rq_idx + 1);

		if(((long) rqe->rq_step) > step) {
			if(step >= 0 && rq_idx > step_stats.start_rq_idx) {
				tse_report_step(&step_stats, wait_times, exec_times, idx, rq_idx);
			}

			/* Save values at the beginning of step */
			memset(&step_stats, 0, sizeof(struct report_stats));
			step_stats.start_rq_idx = rq_idx;
			step_stats.start_idx = idx;
			step = rqe->rq_step;
			snprintf(step_stats.step_name, 8, "%ld", step);
		}

		if(rqe->rq_flags & RQF_FINISHED) {
			tse_process_rq_time(rqe, wait_times, exec_times, idx);
			++idx;
		}

		tse_process_rq_flags(rqe, &step_stats, step);
		tse_process_rq_flags(rqe, &total_stats, step);
	}

	tse_report_step(&step_stats, wait_times, exec_times, idx, rq_idx);
	tse_report_step(&total_stats, wait_times, exec_times, idx, rq_idx);

	puts("\n");

	mp_free(wait_times);
	mp_free(exec_times);

	mp_free(entry);
}

int tse_report(experiment_t* root, int argc, char* argv[]) {
	int flags = EXP_OPEN_RQPARAMS;
	int c;

	while((c = plat_getopt(argc, argv, "S")) != -1) {
		switch(c) {
		case 'S':
			/* Undocumented option for compability with run-tsload output */
			flags = EXP_OPEN_SCHEMA_READ;
			break;
		case '?':
			fprintf(stderr, "Invalid show suboption -%c\n", c);
			return CMD_INVALID_OPT;
		}
	}

	return tse_report_common(root, argc, argv,
						     flags, tse_report_workload, NULL);
}


struct tse_export_option {
	AUTOSTRING char* wl_name;
	AUTOSTRING char* option;

	list_node_t node;
};

struct tse_export_context {
	boolean_t have_dest;
	boolean_t external_dest;
	char dest_path[PATHMAXLEN];

	char backend_name[8];

	list_head_t options;
};

void tse_export_workload(experiment_t* exp, exp_workload_t* ewl, void* context) {
	struct tse_export_context* ctx = (struct tse_export_context*) context;
	struct tse_export_option* opt;

	tsf_backend_t* backend;

	uint32_t rq_count;

	char filename[PATHPARTMAXLEN];
	char path[PATHMAXLEN];

	FILE* file;

	json_node_t* j_hostname;
	char* hostname = NULL;

	if(!ctx->have_dest) {
		/* -d flag was not provided - use experiment dir. Couldn't do this
		 * inside tse_export, cause it has no experiment we get it in tse_report_common() */
		path_join(ctx->dest_path, PATHMAXLEN, exp->exp_root, exp->exp_directory, NULL);
		ctx->have_dest = B_TRUE;
	}

	j_hostname = experiment_cfg_find(exp->exp_config, "agent:hostname", NULL, JSON_STRING);
	if(j_hostname != NULL)
		hostname = json_as_string(j_hostname);

	if(ctx->external_dest) {
		if(hostname != NULL) {
			snprintf(filename, PATHPARTMAXLEN, "%s-%s-%d-%s.%s", hostname,
					 exp->exp_name, exp->exp_runid, ewl->wl_name, ctx->backend_name);
		}
		else {
			snprintf(filename, PATHPARTMAXLEN, "%s-%d-%s.%s",
					 exp->exp_name, exp->exp_runid, ewl->wl_name, ctx->backend_name);
		}
	}
	else {
		snprintf(filename, PATHPARTMAXLEN, "%s.%s", ewl->wl_name, ctx->backend_name);
	}

	path_join(path, PATHMAXLEN, ctx->dest_path, filename, NULL);

	backend = tsfile_backend_create(ctx->backend_name);

	if(backend == NULL) {
		fprintf(stderr, "Couldn't create backend '%s'\n", ctx->backend_name);
		return;
	}

	file = fopen(path, "w");

	if(file == NULL) {
		fprintf(stderr, "Couldn't export workload '%s' - error opening file\n", ewl->wl_name);
		tsfile_backend_destroy(backend);
		return;
	}

	/* Walk over options and apply them */
	list_for_each_entry(struct tse_export_option, opt, &ctx->options, node) {
		if(opt->wl_name == NULL || strcmp(opt->wl_name, ewl->wl_name) == 0) {
			tsfile_backend_set(backend, opt->option);
		}
	}

	rq_count = tsfile_get_count(ewl->wl_file);

	tsfile_backend_set_files(backend, file, ewl->wl_file);
	tsfile_backend_get(backend, 0, rq_count);

	tsfile_backend_destroy(backend);

	fprintf(stderr, "Exported workload '%s' to file %s\n", ewl->wl_name, path);
}

static void tse_add_option(list_head_t* options, const char* optarg) {
	struct tse_export_option* opt = mp_malloc(sizeof(struct tse_export_option));

	const char* colon = strchr(optarg, ':');
	const char* eq = strchr(optarg, '=');

	size_t wl_name_len = 0;
	size_t opt_len;

	/* Colon may be also passed as argument of option, so
	 * ensure if it goes before '=' symbol (inside option name) */
	if(colon != NULL && (eq == NULL || colon < eq)) {
		wl_name_len = colon - optarg;

		aas_copy_n(aas_init(&opt->wl_name), optarg, wl_name_len);
		optarg = colon + 1;
	}

	aas_copy(aas_init(&opt->option), optarg);

	list_node_init(&opt->node);
	list_add_tail(&opt->node, options);
}

static void tse_destroy_options(list_head_t* options) {
	struct tse_export_option* opt;
	struct tse_export_option* opt_next;

	list_for_each_entry_safe(struct tse_export_option, opt, opt_next, options, node) {
		aas_free(&opt->wl_name);
		aas_free(&opt->option);
		mp_free(opt);
	}
}

int tse_export(experiment_t* root, int argc, char* argv[]) {
	struct tse_export_context ctx;

	int flags = EXP_OPEN_SCHEMA_READ;
	int c;

	boolean_t Fflag = B_FALSE;

	int ret;

	ctx.have_dest = B_FALSE;
	ctx.external_dest = B_FALSE;

	list_head_init(&ctx.options, "tse_export_opts");

	while((c = plat_getopt(argc, argv, "F:o:d:")) != -1) {
		switch(c) {
		case 'F':
			strncpy(ctx.backend_name, optarg, 8);
			Fflag = B_TRUE;
			break;
		case 'd':
			strncpy(ctx.dest_path, optarg, PATHMAXLEN);
			ctx.have_dest = B_TRUE;
			ctx.external_dest = B_TRUE;
			break;
		case 'o':
			tse_add_option(&ctx.options, optarg);
			break;
		case '?':
			if(optopt == 'F' || optopt == 'd' || optopt == 'o')
				fprintf(stderr, "-%c option requires an argument\n", optopt);
			else
				fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			return CMD_INVALID_OPT;
		}
	}

	if(!Fflag) {
		strcpy(ctx.backend_name, "json");
	}

	ret = tse_report_common(root, argc, argv,
						    flags, tse_export_workload, &ctx);

	tse_destroy_options(&ctx.options);

	return ret;
}


