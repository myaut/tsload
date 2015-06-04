
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



#ifndef COMMANDS_H_
#define COMMANDS_H_

#include <tsload/defs.h>

#include <experiment.h>

#define CMD_OK		    	0
#define CMD_MISSING_CMD    	-1
#define CMD_UNKNOWN_CMD    	-2
#define CMD_INVALID_OPT  	-3
#define CMD_INVALID_ARG  	-4
#define CMD_MISSING_ARG  	-5

#define CMD_ERROR_BASE		-10
#define CMD_NOT_EXISTS		-11
#define CMD_NO_PERMS		-12
#define CMD_ALREADY_EXISTS	-13
#define CMD_IS_ROOT			-14

#define CMD_GENERIC_ERROR	-30

int tse_experr_to_cmderr(unsigned experr);
int tse_command_error_msg(int code, const char* format, ...);

size_t tse_exp_print_start_time(experiment_t* exp, char* date, size_t buflen);
const char* tse_exp_get_status_str(experiment_t* exp);

int tse_do_command(const char* path, int argc, char* argv[]);

int tse_shift_experiment_run(experiment_t* root, experiment_t** pexp, int argc, char* argv[]);

int tse_list_wltypes(experiment_t* root, int argc, char* argv[]);
int tse_info(experiment_t* root, int argc, char* argv[]);

int tse_list(experiment_t* root, int argc, char* argv[]);
int tse_show(experiment_t* root, int argc, char* argv[]);
int tse_report(experiment_t* root, int argc, char* argv[]);
int tse_export(experiment_t* root, int argc, char* argv[]);
int tse_run(experiment_t* root, int argc, char* argv[]);
int tse_exp_err(experiment_t* root, int argc, char* argv[]);

int run_init(void);
void run_fini(void);

#endif /* COMMANDS_H_ */

