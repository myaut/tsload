/*
 * main.c
 *
 *  Created on: Nov 19, 2012
 *      Author: myaut
 */

#define LOG_SOURCE "run-tsload"
#include <log.h>

#include <mempool.h>
#include <modules.h>
#include <tsload.h>
#include <threads.h>
#include <getopt.h>
#include <pathutil.h>
#include <tsversion.h>
#include <hiobject.h>
#include <tsfile.h>
#include <execpath.h>
#include <geninstall.h>

#include <steps.h>
#include <commands.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

boolean_t mod_configured = B_FALSE;
boolean_t log_configured = B_FALSE;

extern char experiment_dir[];

LIBIMPORT char log_filename[];
LIBIMPORT int log_debug;
LIBIMPORT int log_trace;

LIBIMPORT int mod_type;

LIBIMPORT char mod_search_path[];

LIBEXPORT struct subsystem pre_subsys[] = {
	SUBSYSTEM("load", load_init, load_fini),
};

LIBEXPORT struct subsystem post_subsys[] = {
	SUBSYSTEM("tsfile", tsfile_init, tsfile_fini),
	SUBSYSTEM("hiobject", hi_obj_init, hi_obj_fini),
	SUBSYSTEM("steps", steps_init, steps_fini)
};


enum {
	CMD_NONE,
	CMD_INFO,
	CMD_LOAD
} command;

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
		path_join(log_filename, LOGFNMAXLEN, root, INSTALL_VAR, "run-tsload.log", NULL);
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
	boolean_t eflag = B_FALSE;
	boolean_t ok = B_TRUE;

	time_t now;

	int c;

	char logname[48];

	while((c = plat_getopt(argc, argv, "e:r:dtmhv")) != -1) {
		switch(c) {
		case 'h':
			usage(0, "");
			break;
		case 'e':
			eflag = B_TRUE;
			command = CMD_LOAD;
			strncpy(experiment_dir, optarg, PATHMAXLEN);

			/* In experiment mode we write logs into experiment directory */
			now = time(NULL);

			strftime(logname, 48, "run-tsload-%Y-%m-%d-%H_%M_%S.log", localtime(&now));
			path_join(log_filename, LOGFNMAXLEN, experiment_dir, logname, NULL);

			break;
		case 'm':
			command = CMD_INFO;
			break;
		case 't':
			log_trace = 1;
			/*FALLTHROUGH*/
		case 'd':
			log_debug = 1;
			break;
		case 'v':
			print_ts_version("loader tool (standalone)");
			exit(0);
			break;
		case '?':
			if(optopt == 'e' || optopt == 'r')
				usage(1, "-%c option requires an argument\n", optopt);
			else
				usage(1, "Unknown option `-%c'.\n", optopt);
			break;
		}
	}

	if(!eflag && !log_configured) {
		usage(1, "Missing TS_LOGFILE environment variable, and not configured in experiment mode\n");
	}

}

int main(int argc, char* argv[]) {
	int err = 0;

	command = CMD_NONE;

	deduce_paths();
	read_environ();
	parse_options(argc, argv);

	if(!mod_configured) {
		usage(1, "Missing TS_MODPATH environment variable\n");
	}

	mod_type = MOD_TSLOAD;

	atexit(ts_finish);
	tsload_init(pre_subsys, 1, post_subsys, 2);

	logmsg(LOG_INFO, "Started run-tsload");
	logmsg(LOG_DEBUG, "run-tsload command: %d", command);

	if(command == CMD_INFO) {
		err = do_info();
	}
	else if(command == CMD_LOAD) {
		err = do_load();
	}
	else {
		usage(1, "Command not specified");
	}

	if(err != 0) {
		fprintf(stderr, "Error encountered, see log for details\n");
	}

	return 0;
}


