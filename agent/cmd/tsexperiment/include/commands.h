/*
 * commands.h
 *
 *  Created on: Feb 8, 2014
 *      Author: myaut
 */

#ifndef COMMANDS_H_
#define COMMANDS_H_

#include <experiment.h>

#define CMD_OK		    	0
#define CMD_UNKNOWN      	-1
#define CMD_MISSING_ARG  	-2
#define CMD_INVALID_PATH	-3
#define CMD_INVALID_OPT  	-4
#define CMD_INVALID_ARG  	-5
#define CMD_ERROR			-6

int tse_do_command(const char* path, int argc, char* argv[]);

experiment_t* tse_shift_experiment_run(experiment_t* root, int argc, char* argv[]);

int tse_list_wltypes(experiment_t* root, int argc, char* argv[]);

int tse_list(experiment_t* root, int argc, char* argv[]);
int tse_show(experiment_t* root, int argc, char* argv[]);
int tse_report(experiment_t* root, int argc, char* argv[]);
int tse_export(experiment_t* root, int argc, char* argv[]);
int tse_run(experiment_t* root, int argc, char* argv[]);

int run_init(void);
void run_fini(void);

#endif /* COMMANDS_H_ */
