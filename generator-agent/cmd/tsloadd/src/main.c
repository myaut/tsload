/*
 * main.c
 *
 * Main file for ts-loader daemon
 */


#define LOG_SOURCE "tsloadd"
#include <log.h>

#include <modtsload.h>
#include <cfgfile.h>
#include <tsload.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <signal.h>
#include <unistd.h>

char config_file_name[CONFPATHLEN];

extern int log_debug;
extern int log_trace;


void usage() {
	fprintf(stderr, "command line: \n");
	fprintf(stderr, "\ttsloadd -f <config-file> [-d|-t]\n");

	exit(1);
}

void parse_options(int argc, char* argv[]) {
	int fflag = 0;
	int ok = 1;

	int c;

	while((c = getopt(argc, argv, "f:dt")) != -1) {
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

void sigusr1_handler(int sig) {
#if 0
	t_dump_threads();
#endif
}

int main(int argc, char* argv[]) {
	int err = 0;

	set_mod_helper(MOD_TSLOAD, tsload_mod_helper);
	parse_options(argc, argv);

	sigset(SIGUSR1, sigusr1_handler);

	if((err = cfg_init(config_file_name)) != CFG_OK) {
		return 1;
	}

	tsload_start(argv[0]);

	return 0;
}
