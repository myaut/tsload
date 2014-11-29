
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


#define TSEXPERIMENT_LOGFILE		"tsexperiment.log"

boolean_t eflag = B_FALSE;

boolean_t mod_configured = B_FALSE;
boolean_t log_configured = B_FALSE;

static char experiment_root_path[PATHMAXLEN];

LIBIMPORT char log_filename[];
LIBIMPORT int mod_type;

LIBIMPORT char mod_search_path[];

LIBEXPORT struct subsystem pre_subsys[] = {
	SUBSYSTEM("error", tse_error_init, tse_error_fini),
};

LIBEXPORT struct subsystem post_subsys[] = {
	SUBSYSTEM("tsfile", tsfile_init, tsfile_fini),
	SUBSYSTEM("hiobject", hi_obj_init, hi_obj_fini),
	SUBSYSTEM("run", run_init, run_fini),
};

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

	const char* cur_dirpath = path_dirname(&iter, cur_execpath);

	if(path_remove(root, PATHMAXLEN, cur_dirpath, INSTALL_BIN) != NULL) {
		path_join(log_filename, LOGFNMAXLEN, root, INSTALL_VAR, TSEXPERIMENT_LOGFILE, NULL);
		path_join(mod_search_path, MODPATHLEN, root, INSTALL_MOD_LOAD, NULL);

		mod_configured = B_TRUE;
		log_configured = B_TRUE;
	}
}

void read_environ() {
	char* env_mod_path = getenv("TS_MODPATH");
	char* env_log_filename = getenv("TS_LOGFILE");

	if(env_mod_path) {
		strncpy(mod_search_path, env_mod_path, MODPATHLEN);
		mod_configured = B_TRUE;
	}

	if(env_log_filename) {
		strncpy(log_filename, env_log_filename, LOGFNMAXLEN);
		log_configured = B_TRUE;
	}
}

void parse_options(int argc, const char* argv[]) {
	int c;

	while((c = plat_getopt(argc, argv, "+e:X:hv")) != -1) {
		switch(c) {
		case 'h':
			usage(0, "");
			break;
		case 'e':
			eflag = B_TRUE;
			strncpy(experiment_root_path, optarg, PATHMAXLEN);
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

	const char* exp_root_path = NULL;

	opterr = 0;

	deduce_paths();
	read_environ();
	parse_options(argc, argv);

	if(!mod_configured) {
		usage(1, "Missing TS_MODPATH environment variable and failed to deduce modpath\n");
	}

	if(!log_configured) {
		if(eflag) {
			path_join(log_filename, LOGFNMAXLEN, experiment_root_path, TSEXPERIMENT_LOGFILE, NULL);
		}
		else {
			usage(1, "Failed to configure log. Use TS_LOGFILE to set it explicitly.\n");
		}
	}

	mod_type = MOD_TSLOAD;

	atexit(ts_finish);
	tsload_init(pre_subsys, sizeof(pre_subsys) / sizeof(struct subsystem),
				post_subsys, sizeof(post_subsys) / sizeof(struct subsystem));

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

