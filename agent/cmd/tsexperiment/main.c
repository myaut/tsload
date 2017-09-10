
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



#define LOG_SOURCE "tsexperiment"
#include <tsload/log.h>

#include <tsload/defs.h>

#include <tsload/pathutil.h>
#include <tsload/execpath.h>
#include <tsload/getopt.h>
#include <tsload/version.h>
#include <tsload/tuneit.h>
#include <tsload/autostring.h>
#include <tsload/geninstall.h>

#include <tsload/json/json.h>

#include <tsload.h>

#include <hostinfo/hiobject.h>

#include <commands.h>
#include <tseerror.h>
#include <experiment.h>

#define TSEXPERIMENT_LOGFILE		"tsexperiment.log"

boolean_t eflag = B_FALSE;

static char experiment_root_path[PATHMAXLEN];
static char log_filename[PATHMAXLEN];
static char mod_search_path[PATHMAXLEN];
static char hi_obj_modpath[PATHMAXLEN];

int init(void);
void usage(int ret, const char* reason, ...);

/*
 * Make TSLoad a bit portable - get absolute path of run-tsload,
 * delete INSTALL_BIN from it and append INSTALL_VAR or INSTALL_MOD_PATH
 * to find path to log and modules.
 * */
void deduce_paths(void) {
	const char* cur_execpath = plat_execpath();
	path_split_iter_t iter;
	char root[PATHMAXLEN];
	char path[PATHMAXLEN];

	const char* cur_dirpath = path_dirname(&iter, cur_execpath);

	if(path_remove(root, PATHMAXLEN, cur_dirpath, INSTALL_BIN) != NULL) {
		path_join(log_filename, PATHMAXLEN, root, INSTALL_VAR, TSEXPERIMENT_LOGFILE, NULL);
		setenv("TS_LOGFILE", log_filename, B_FALSE);
		
		path_join(mod_search_path, PATHMAXLEN, root, INSTALL_MOD_LOAD, NULL);
		setenv("TS_MODPATH", mod_search_path, B_FALSE);
		
		path_join(hi_obj_modpath, PATHMAXLEN, root, INSTALL_LIB, NULL);
		setenv("TS_HIMODPATH", hi_obj_modpath, B_FALSE);
	}
}

void parse_options(int argc, char* const argv[]) {
	int c;

	while((c = plat_getopt(argc, argv, "+e:X:hv")) != -1) {
		switch(c) {
		case 'h':
			usage(0, "");
			break;
		case 'e':
			eflag = B_TRUE;
			path_argfile(experiment_root_path, PATHMAXLEN, 
						 EXPERIMENT_FILENAME, optarg);
			break;
		case 'X':
			tuneit_add_option(optarg);
			break;
		case 'v':
			print_ts_version("TS Experiment - standalone agent to run TSLoad experiments ");
			exit(0);
			break;
		case '?':
			if(optopt == 'e')
				usage(-CMD_MISSING_ARG, "-%c option requires an argument\n", optopt);
			else
				usage(-CMD_INVALID_OPT, "Unknown option `-%c'.\n", optopt);
			break;
		}
	}
}

int main(int argc, char* argv[]) {
	int err = 0;
	boolean_t has_logfile = B_FALSE;

	const char* exp_root_path = NULL;

	opterr = 0;
	
	/* If TS_LOGFILE was explicitly set as environment variable, we keep it.
	 * If not, we overwrite it in deduce_paths() and later, while checking for eflag */
	has_logfile = (getenv("TS_LOGFILE") != NULL);
	
	deduce_paths();
	parse_options(argc, argv);

	if(getenv("TS_MODPATH") == NULL) {
		usage(1, "Missing TS_MODPATH environment variable and failed to deduce modpath\n");
	}
	
	if(getenv("TS_MODPATH") == NULL) {
		usage(1, "Missing TS_HIMODPATH environment variable and failed to deduce HostInfo modpath\n");
	}

	if(eflag) {
		path_join(log_filename, PATHMAXLEN, experiment_root_path, TSEXPERIMENT_LOGFILE, NULL);
		setenv("TS_LOGFILE", log_filename, !has_logfile);
	}
	else if(getenv("TS_LOGFILE") == NULL) {
		usage(1, "Failed to configure log file name. Use TS_LOGFILE to set it explicitly.\n");
	}

	init();

	logmsg(LOG_INFO, "Started TSExperiment");

	argc -= optind;
	argv = &argv[optind];
	optind = 1;

	if(eflag) {
		exp_root_path = experiment_root_path;
	}

	err = tse_do_command(exp_root_path, argc, argv);

	if(err < 0 && err > CMD_ERROR_BASE) {
		usage(-err, "Invalid command line option or argument\n");
	}
	if(err < 0) {
		err = -err;
	}

	return err;
}

