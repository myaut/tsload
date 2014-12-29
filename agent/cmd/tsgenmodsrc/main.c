/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, Tune-IT

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

#include <tsload/defs.h>

#include <tsload/mempool.h>
#include <tsload/getopt.h>
#include <tsload/version.h>
#include <tsload/pathutil.h>
#include <tsload/execpath.h>
#include <tsload/geninstall.h>

#include <tsload/log.h>
#include <tsload/init.h>
#include <tsload/threads.h>
#include <tsload/log.h>
#include <tsload/mempool.h>
#include <tsload/json/json.h>

#include <genmodsrc.h>
#include <preproc.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <tsload/plat/posixdecl.h>

LIBIMPORT char log_filename[];

char modinfo_path[PATHMAXLEN];
char root_path[PATHMAXLEN];
char devel_path[PATHPARTMAXLEN];
char include_path[PATHPARTMAXLEN];

extern boolean_t modpp_trace;

int build_system = MOD_BUILD_SCONS;
int module_type  = MOD_EXTERNAL;
int command = 0;

void usage(int ret, const char* reason, ...);

static void parse_options(int argc, char* argv[]) {
	int ok = 1;
	int argi;
	int c;

	while((c = plat_getopt(argc, argv, "hvb:t:T")) != -1) {
		switch(c) {
		case 'v':
			print_ts_version("Module source generator");
			exit(0);
			break;
		case 'b':
			if(strcmp(optarg, "scons") == 0) {
				build_system = MOD_BUILD_SCONS;
			}
			else if(strcmp(optarg, "make") == 0) {
				build_system = MOD_BUILD_MAKE;
			}
			else {
				usage(1, "Invalid build system '%s'\n", optarg);
			}
			break;
		case 'T':
			modpp_trace = B_TRUE;
			break;
		case 't':
			if(strcmp(optarg, "int") == 0) {
				module_type = MOD_INTERNAL;
			}
			else if(strcmp(optarg, "ext") == 0) {
				module_type = MOD_EXTERNAL;
			}
			else {
				usage(1, "Invalid build type '%s'\n", optarg);
			}
			break;
		case 'h':
			usage(0, "");
			break;
		case '?':
			usage(1, "Unknown option `-%c'.\n", optopt);
			break;
		}
	}

	if(build_system == MOD_BUILD_MAKE && module_type == MOD_INTERNAL) {
		usage(1, "Invalid arguments: internal modules couln't be built by GNU make\n");
	}

	/* Check subcommand */
	argi = optind;
	if(argi == argc) {
		usage(1, "Missing subcommand\n");
	}

	if(strcmp(argv[optind], "vars") == 0) {
		command = MODSRC_SHOW_VARS;
	}
	else if(strcmp(argv[optind], "header") == 0 || strcmp(argv[optind], "hdr") == 0) {
		command = MODSRC_SHOW_HEADER;
	}
	else if(strcmp(argv[optind], "source") == 0 || strcmp(argv[optind], "src") == 0) {
		command = MODSRC_SHOW_SOURCE;
	}
	else if(strcmp(argv[optind], "makefile") == 0) {
		command = MODSRC_SHOW_MAKEFILE;
	}
	else if(strcmp(argv[optind], "generate") == 0) {
		command = MODSRC_GENERATE;
	}
	else {
		usage(1, "Invalid subcommand\n");
	}

	/* Read modinfo path */
	++argi;
	if(argi == argc) {
		usage(1, "Missing modinfo.json path\n");
	}

	strncpy(modinfo_path, argv[argi], PATHMAXLEN);
}

/* Builds path relative to root from subpath and extrapath (may be NULL).
 *
 * If it exists and accessible (access is set by mask)
 * copies subpath to dest and returns B_TRUE. Otherwise, returns B_FALSE.
 */
static boolean_t set_path(char* dest, size_t len, const char* root, const char* subpath,
						  const char* extrapath, int mask) {
	char path[PATHMAXLEN];

	if(subpath == NULL)
		return B_FALSE;

	path_join(path, PATHMAXLEN, root, subpath, extrapath, NULL);

	if(access(path, mask) == 0) {
		path_join(dest, len, subpath, extrapath, NULL);
		return B_TRUE;
	}

	return B_FALSE;
}

void set_paths(const char* root) {
	char share_path[PATHPARTMAXLEN];

	if(root == NULL) {
		fprintf(stderr, "Cannot deduce root path, set TSLOADPATH\n");
		exit(1);
	}

	strncpy(root_path, root, PATHMAXLEN);

	if(! (set_path(share_path, PATHPARTMAXLEN, root,
				   INSTALL_SHARE, NULL, R_OK | X_OK) ||
		  set_path(share_path, PATHPARTMAXLEN, root,
				   getenv("TS_INSTALL_SHARE"), NULL, R_OK | X_OK) ) ) {
		fprintf(stderr, "Cannot access share path '%s', set TS_INSTALL_SHARE to correct subpath\n",
				share_path);
		exit(1);
	}

	if(! set_path(devel_path, PATHPARTMAXLEN, root,
			      share_path, DEVEL_DIR, R_OK | X_OK)) {
		fprintf(stderr, "Cannot access devel path '%s', install devel files\n", devel_path);
		exit(1);
	}

	if(! (set_path(include_path, PATHPARTMAXLEN, root,
			       INSTALL_INCLUDE, NULL, R_OK | X_OK) ||
		  set_path(include_path, PATHPARTMAXLEN, root,
				   getenv("TS_INSTALL_INCLUDE"), NULL, R_OK | X_OK) ) ) {
		fprintf(stderr, "Cannot access include path '%s', install devel files "
				"or set TS_INSTALL_INCLUDE\n", devel_path);
		exit(1);
	}
}

static void deduce_paths(void) {
	const char* cur_execpath = plat_execpath();
	path_split_iter_t iter;
	char root[PATHMAXLEN];

	const char* cur_dirpath = path_dirname(&iter, cur_execpath);

	if(path_remove(root, PATHMAXLEN, cur_dirpath, INSTALL_BIN) != NULL) {
		set_paths(root);
	}
	else {
		set_paths(getenv("TSLOADPATH"));
	}
}

int tsgenmodsrc_pp(const char* fname, FILE* outf) {
	char path[PATHMAXLEN];
	FILE* inf;
	int err = MODPP_OK;

	path_join(path, PATHMAXLEN, devel_path, MODSRC_DIR,
			  fname, NULL);

	inf = fopen(path, "r");

	if(inf == NULL) {
		fprintf(stderr, "Cannot open template file '%s'", path);
		return 1;
	}

	modpp_subst(inf, outf);

	fclose(inf);

	return 0;
}

int tsgenmodsrc_init(void) {
	struct subsystem subsys[] = {
		SUBSYSTEM("log", log_init, log_fini),
		SUBSYSTEM("mempool", mempool_init, mempool_fini),
		SUBSYSTEM("threads", threads_init, threads_fini),
		SUBSYSTEM("json", json_init, json_fini),
		SUBSYSTEM("modsrc", modsrc_init, modsrc_fini)
	};

	int count = sizeof(subsys) / sizeof(struct subsystem);
	int i = 0;

	struct subsystem** subsys_list = (struct subsystem**)
			malloc(count * sizeof(struct subsystem*));

	for(i = 0; i < count; ++i) {
		subsys_list[i] = &subsys[i];
	}

	atexit(ts_finish);
	return ts_init(subsys_list, count);
}

int main(int argc, char* argv[]) {
	int err = 0;
	int i;

	parse_options(argc, argv);

	/* Check if load config accessible */
	if(access(modinfo_path, R_OK) != 0) {
		fprintf(stderr, "Loader config file '%s' is not accessible or not exists.\n", modinfo_path);
		return 1;
	}

	deduce_paths();

	strncpy(log_filename, "-", LOGFNMAXLEN);
	tsgenmodsrc_init();

	modvar_set(modvar_create("TSLOADPATH"), root_path);
	modvar_set(modvar_create("TS_INSTALL_INCLUDE"), include_path);
	modvar_set(modvar_create("TS_INSTALL_DEVEL"), devel_path);

	if(command == MODSRC_GENERATE && modinfo_check_dir(modinfo_path) != 0) {
		return 1;
	}

	if(modinfo_read_config(modinfo_path) != 0) {
		return 1;
	}

	switch(command) {
	case MODSRC_SHOW_VARS:
		modsrc_dump_vars();
		break;
	case MODSRC_SHOW_HEADER:
		tsgenmodsrc_pp(HEADER_IN_FN, stdout);
		break;
	case MODSRC_SHOW_SOURCE:
		tsgenmodsrc_pp(SOURCE_IN_FN, stdout);
		break;
	}

	return 0;
}
