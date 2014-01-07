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

void usage() {
	fprintf(stderr, "environment options: \n"
					"\tTS_MODPATH - path to loadable modules\n"
					"\tTS_LOGFILE - log destination ('-' for stdout)\n"
					"command line: \n"
					"\trun-tsload [-d|-t] -e <experiment directory> \n\t\truns loader\n"
					"\trun-tsload [-d|-t] -m \n\t\tget information on modules (in JSON)\n"
					"\trun-tsload [-d|-t] -h \n\t\thelp\n"
					"\trun-tsload -v \n\t\tversion information\n");

	exit(1);
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
			usage();
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
				fprintf(stderr, "-%c option requires an argument\n", optopt);
			else
				fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			ok = B_FALSE;
			break;
		}

		if(!ok)
			break;
	}

	if(!eflag && !log_configured) {
		fprintf(stderr, "Missing TS_LOGFILE environment variable, and not configured in experiment mode\n");
		usage();
	}

	if(!ok) {
		usage();
	}
}

int main(int argc, char* argv[]) {
	int err = 0;

	command = CMD_NONE;

	read_environ();
	parse_options(argc, argv);

	if(!mod_configured) {
		fprintf(stderr, "Missing TS_MODPATH environment variable\n");
		usage();
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
		usage();
	}

	if(err != 0) {
		fprintf(stderr, "Error encountered, see log for details\n");
	}

	return 0;
}


