/*
 * main.c
 *
 *  Created on: Feb 8, 2014
 *      Author: myaut
 */

#define LOG_SOURCE "tsexperiment"
#include <log.h>

#include <pathutil.h>
#include <execpath.h>
#include <geninstall.h>
#include <getopt.h>
#include <tsload.h>
#include <tsversion.h>
#include <tuneit.h>

#include <json.h>

#include <tsfile.h>
#include <hiobject.h>

#include <modules.h>

#include <commands.h>

#define TSEXPERIMENT_LOGFILE		"tsexperiment.log"

boolean_t eflag = B_FALSE;

boolean_t mod_configured = B_FALSE;
boolean_t log_configured = B_FALSE;

static char experiment_root_path[PATHMAXLEN];

LIBIMPORT char log_filename[];
LIBIMPORT int mod_type;

LIBIMPORT char mod_search_path[];

LIBEXPORT struct subsystem pre_subsys[] = {
	SUBSYSTEM("run", run_init, run_fini),
};

LIBEXPORT struct subsystem post_subsys[] = {
	SUBSYSTEM("json", json_init, json_fini),
	SUBSYSTEM("tsfile", tsfile_init, tsfile_fini),
	SUBSYSTEM("hiobject", hi_obj_init, hi_obj_fini)
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
				usage(1, "-%c option requires an argument\n", optopt);
			else
				usage(1, "Unknown option `-%c'.\n", optopt);
			break;
		}
	}
}

void tse_error_msg(ts_errcode_t code, const char* format, ...) {
	va_list va;
	char fmtstr[256];

	snprintf(fmtstr, 256, "ERROR %d: %s\n", code, format);

	va_start(va, format);
	vfprintf(stderr, fmtstr, va);
	va_end(va);
}


int main(int argc, char* argv[]) {
	int err = 0;

	const char* exp_root_path = NULL;

	deduce_paths();
	read_environ();
	parse_options(argc, argv);

	if(!mod_configured) {
		usage(1, "Missing TS_MODPATH environment variable and failed to deduce modpath\n");
	}

	if(eflag) {
		path_join(log_filename, LOGFNMAXLEN, experiment_root_path, TSEXPERIMENT_LOGFILE, NULL);
	}
	else if(!log_configured) {
		usage(1, "Failed to configure log. Use TS_LOGFILE to set it explicitly.\n");
	}

	mod_type = MOD_TSLOAD;

	atexit(ts_finish);
	tsload_init(pre_subsys, 1, post_subsys, 2);

	logmsg(LOG_INFO, "Started TSExperiment");

	tsfile_error_msg = tse_error_msg;

	argc -= optind;
	argv = &argv[optind];
	optind = 1;

	if(eflag) {
		exp_root_path = experiment_root_path;
	}

	err = tse_do_command(exp_root_path, argc, argv);

	if(err != 0) {
		usage(err, "Error encountered, see log for details\n");
	}

	return err;
}
