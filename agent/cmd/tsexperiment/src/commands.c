
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



#include <getopt.h>
#include <tstime.h>

#include <experiment.h>
#include <commands.h>

#include <stdio.h>
#include <string.h>

typedef int (*tse_command_func)(experiment_t* root, int argc, char* argv[]);

typedef struct tse_command {
	const char* name;
	tse_command_func func;
	boolean_t needroot;
} tse_command_t;

tse_command_t commands[] = {
	{"workload", tse_list_wltypes, B_FALSE},
	{"wl", tse_list_wltypes, B_FALSE},
	{"list", tse_list, B_TRUE},
	{"show", tse_show, B_TRUE},
	{"report", tse_report, B_TRUE},
	{"export", tse_export, B_TRUE},
	{"run", tse_run, B_TRUE},
	{NULL, NULL}
};

int tse_do_command(const char* path, int argc, char* argv[]) {
	tse_command_t* command = &commands[0];
	int ret = CMD_UNKNOWN;
	experiment_t* root = NULL;

	if(argc == 0) {
		fputs("Missing subcommand!\n", stderr);
		return CMD_MISSING_ARG;
	}

	if(path) {
		root = experiment_load_root(path);
		if(root == NULL) {
			fprintf(stderr, "Couldn't load experiment from '%s'", path);
			return CMD_INVALID_PATH;
		}
	}

	while(command->name != NULL) {
		if(strcmp(command->name, argv[0]) == 0) {
			if(!root && command->needroot) {
				fprintf(stderr, "Subcommand '%s' requires experiment root\n", command->name);
				return CMD_MISSING_ARG;
			}

			ret = command->func(root, argc, argv);
			break;
		}

		++command;
	}

	if(root) {
		experiment_destroy(root);
	}

	return ret;
}

experiment_t* tse_shift_experiment_run(experiment_t* root, int argc, char* argv[]) {
	int argi = optind++;
	int runid;

	experiment_t* exp = NULL;

	if(argi < argc) {
		runid = strtol(argv[argi], NULL, 10);

		if(runid == -1) {
			fprintf(stderr, "Invalid runid '%s' - should be integer\n", argv[argi]);
			return NULL;
		}

		exp = experiment_load_run(root, runid);

		if(exp == NULL) {
			fprintf(stderr, "Couldn't open experiment run with runid %d\n", runid);
			return NULL;
		}
	}

	return exp;
}

size_t tse_exp_print_start_time(experiment_t* exp, char* date, size_t buflen) {
	JSONNODE* start_time;
	ts_time_t start_time_tm;

	/* Get date */
	start_time = experiment_cfg_find(exp->exp_config, "start_time", NULL);
	if(start_time != NULL) {
		start_time_tm = json_as_int(start_time);
		return tm_datetime_print(start_time_tm, date, buflen);
	}

	return strcpy(date, "???");
}

const char* tse_exp_get_status_str(experiment_t* exp) {
	JSONNODE* status;
	int status_code = EXPERIMENT_UNKNOWN;

	if(exp->exp_status == EXPERIMENT_UNKNOWN) {
		status = experiment_cfg_find(exp->exp_config, "status", NULL);
		if(status != NULL) {
			status_code = json_as_int(status);
		}
	}
	else {
		status_code = exp->exp_status;
	}

	switch(status_code) {
		case EXPERIMENT_OK:
			return "OK";
		case EXPERIMENT_ERROR:
			return "ERROR";
			break;
		case EXPERIMENT_FINISHED:
			return "FINISHED";
		case EXPERIMENT_NOT_CONFIGURED:
			return "NOTCFG";
	}

	return "UNKNOWN";
}



