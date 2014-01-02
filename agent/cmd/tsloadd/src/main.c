/*
 * main.c
 *
 * Main file for ts-loader daemon
 */


#define LOG_SOURCE "tsloadd"
#include <log.h>

#include <loadagent.h>
#include <modules.h>
#include <cfgfile.h>
#include <tsload.h>
#include <getopt.h>
#include <tsversion.h>
#include <hiobject.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

LIBIMPORT int log_debug;
LIBIMPORT int log_trace;

LIBIMPORT int mod_type;

char config_file_name[CONFPATHLEN];

#define XSUBSYS_COUNT 			  2

LIBEXPORT struct subsystem xsubsys[XSUBSYS_COUNT] = {
	SUBSYSTEM("hiobject", hi_obj_init, hi_obj_fini),
	SUBSYSTEM("agent", agent_init, agent_fini)
};


void usage() {
	fprintf(stderr, "command line: \n"
					"\ttsloadd -f <config-file> [-d|-t]\n"
					"\ttsloadd -v - tsload version\n");

	exit(1);
}

void parse_options(int argc, char* argv[]) {
	int fflag = 0;
	int ok = 1;

	int c;

	while((c = plat_getopt(argc, argv, "f:dtv")) != -1) {
		switch(c) {
		case 'f':
			fflag = 1;
			strncpy(config_file_name, optarg, CONFPATHLEN);
			break;
		case 't':
			log_trace = 1;
			/*FALLTHROUGH*/
		case 'd':
			log_debug = 1;
			break;
		case 'v':
			print_ts_version("Loader agent (daemon)");
			exit(0);
			break;
		case '?':
			if(optopt == 'l' || optopt == 'm')
				fprintf(stderr, "-%c option requires an argument\n", optopt);
			else
				fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			ok = 0;
			break;
		}

		if(!ok)
			break;
	}

	if(!ok || !fflag) {
		usage();
		exit(1);
	}
}


int main(int argc, char* argv[]) {
	int err = 0;

	mod_type = MOD_TSLOAD;
	parse_options(argc, argv);

	if((err = cfg_init(config_file_name)) != CFG_OK) {
		return 1;
	}

	atexit(ts_finish);
	tsload_init(NULL, 0, xsubsys, XSUBSYS_COUNT);

	tsload_start(argv[0]);

	return 0;
}
