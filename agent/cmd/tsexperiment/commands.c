
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



#include <tsload/defs.h>

#include <tsload/getopt.h>
#include <tsload/time.h>

#include <tsload/json/json.h>

#include <experiment.h>
#include <commands.h>
#include <tseerror.h>

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
	{"info", tse_info, B_FALSE},
	{"list", tse_list, B_TRUE},
	{"show", tse_show, B_TRUE},
	{"report", tse_report, B_TRUE},
	{"export", tse_export, B_TRUE},
	{"run", tse_run, B_TRUE},

	/* Undocumented */
	{"experr", tse_exp_err, B_FALSE},
	{NULL, NULL}
};

int tse_do_command(const char* path, int argc, char* argv[]) {
	tse_command_t* command = &commands[0];
	int ret = CMD_UNKNOWN_CMD;
	experiment_t* root = NULL;

	if(argc == 0) {
		fputs("Missing subcommand!\n", stderr);
		return CMD_MISSING_CMD;
	}

	while(command->name != NULL) {
		if(strcmp(command->name, argv[0]) == 0) {
			if(command->needroot) {
				if(!path) {
					tse_command_error_msg(CMD_MISSING_ARG,
							"Subcommand '%s' requires experiment root\n", command->name);
					return CMD_MISSING_ARG;
				}

				root = experiment_load_root(path);
				if(root == NULL) {
					ret = tse_experr_to_cmderr(experiment_load_error());
					tse_command_error_msg(ret, "Couldn't load experiment from '%s'\n", path);

					return ret;
				}
			}
			else if(path) {
				tse_command_error_msg(CMD_INVALID_OPT,
						"Subcommand '%s' does not need experiment root\n", command->name);
				return CMD_INVALID_OPT;
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

/**
 * Reads next argument from `argv` using `optind` as current index.
 * If no more arguments exist, returns `root`. In case of error, returns
 * `NULL`. If experiment was successfully loaded, returns its config.
 */
int tse_shift_experiment_run(experiment_t* root, experiment_t** pexp, int argc, char* argv[]) {
	int argi = optind++;
	int runid;

	int err;

	experiment_t* exp = root;

	if(argi < argc) {
		char* arg = argv[argi];
		char* endptr = NULL;
		char* end = arg + strlen(arg);
		
		runid = strtol(arg, &endptr, 10);
		
		if(runid < 0 || (runid == 0 && endptr != end)) {
			tse_command_error_msg(CMD_INVALID_ARG,
					"Invalid runid '%s' - should be non-negative integer\n", argv[argi]);
			return CMD_INVALID_ARG;
		}

		exp = experiment_load_run(root, runid);

		if(exp == NULL) {
			err = tse_experr_to_cmderr(experiment_load_error());
			tse_command_error_msg(err, "Couldn't open experiment run with runid %d\n", runid);
			return err;
		}
	}

	*pexp = exp;
	return CMD_OK;
}

size_t tse_exp_print_start_time(experiment_t* exp, char* date, size_t buflen) {
	json_node_t* start_time;
	ts_time_t start_time_tm;

	/* Get date */
	start_time = experiment_cfg_find(exp->exp_config, "start_time", NULL, JSON_NUMBER_INTEGER);
	if(start_time != NULL) {
		start_time_tm = json_as_integer(start_time);
		return tm_datetime_print(start_time_tm, date, buflen);
	}

	return strcpy(date, "???");
}

const char* tse_exp_get_status_str(experiment_t* exp) {
	json_node_t* status;
	int status_code = EXPERIMENT_UNKNOWN;

	if(exp->exp_status == EXPERIMENT_UNKNOWN) {
		status = experiment_cfg_find(exp->exp_config, "status", NULL, JSON_NUMBER_INTEGER);
		if(status != NULL) {
			status_code = json_as_integer(status);
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



