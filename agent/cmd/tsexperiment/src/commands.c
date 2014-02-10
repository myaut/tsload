/*
 * commands.c
 *
 *  Created on: Feb 8, 2014
 *      Author: myaut
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




