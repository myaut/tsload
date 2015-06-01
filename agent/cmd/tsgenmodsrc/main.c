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

#include <tsload/getopt.h>
#include <tsload/version.h>
#include <tsload/pathutil.h>
#include <tsload/execpath.h>
#include <tsload/geninstall.h>
#include <tsload/log.h>

#include <genmodsrc.h>
#include <preproc.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#include <tsload/plat/posixdecl.h>

LIBIMPORT char log_filename[];

char modinfo_dir[PATHMAXLEN];
char modinfo_path[PATHMAXLEN];

char root_path[PATHMAXLEN];
char bldenv_path[PATHPARTMAXLEN];
char devel_path[PATHPARTMAXLEN];
char include_path[PATHPARTMAXLEN];
char mod_load_path[PATHPARTMAXLEN];
char lib_path[PATHPARTMAXLEN];

extern boolean_t modpp_trace;

int build_system = MOD_BUILD_SCONS;
int module_type  = MOD_EXTERNAL;
int command = 0;

modtarget_t tgt_scons_int[] = {
	MOD_TARGET_ROOT(SOURCE_IN_FN, "@SOURCE_FILENAME"),
	MOD_TARGET_ROOT(HEADER_IN_FN, "include", "@HEADER_FILENAME"),
	MOD_TARGET_ROOT(SCONSCRIPT_IN_FN, "@SCONSCRIPT_FILENAME"),
	MOD_TARGET_END
};

modtarget_t tgt_scons_ext[] = {
	MOD_TARGET_MOD(SOURCE_IN_FN, "@SOURCE_FILENAME"),
	MOD_TARGET_MOD(HEADER_IN_FN, "include", "@HEADER_FILENAME"),
	MOD_TARGET_MOD(SCONSCRIPT_IN_FN, "@SCONSCRIPT_FILENAME"),
	MOD_TARGET_ROOT(SCONSTRUCT_IN_FN, "@SCONSTRUCT_FILENAME"),
	MOD_TARGET_END
};

modtarget_t tgt_make[] = {
	MOD_TARGET_ROOT(SOURCE_IN_FN, "@SOURCE_FILENAME"),
	MOD_TARGET_ROOT(HEADER_IN_FN, "include", "@HEADER_FILENAME"),
	MOD_TARGET_ROOT(MAKEFILE_IN_FN, "@MAKEFILE_FILENAME"),
	MOD_TARGET_END
};

modtarget_t tgt_build[] = {
	MOD_TARGET_ROOT(NULL, ".sconsign.dblite"),
	MOD_TARGET_BUILD(NULL, "@SOURCE_FILENAME"),
	MOD_TARGET_BUILD(NULL, "include", "@HEADER_FILENAME"),
	MOD_TARGET_BUILD(NULL, "@SCONSCRIPT_FILENAME"),
	MOD_TARGET_BUILD(NULL, "@OBJECT_FILENAME"),
	MOD_TARGET_BUILD(NULL, "@MODULE_FILENAME"),
	MOD_TARGET_END
};

int init(void);
void usage(int ret, const char* reason, ...);

static void parse_options(int argc, char* const argv[]) {
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
	else if(strcmp(argv[optind], "generate") == 0 || strcmp(argv[optind], "gen") == 0) {
		command = MODSRC_GENERATE;
	}
	else if(strcmp(argv[optind], "clean") == 0) {
		command = MODSRC_CLEAN;
	}
	else {
		usage(1, "Invalid subcommand\n");
	}

	/* Read modinfo path */
	++argi;
	if(argi == argc) {
		usage(1, "Missing modinfo.json path\n");
	}

	path_argfile(modinfo_dir, PATHMAXLEN, 
				 MODINFO_FILENAME, argv[argi]);
	path_join(modinfo_path, PATHMAXLEN, 
			  modinfo_dir, MODINFO_FILENAME, NULL);
}

/* Builds path relative to root from subpath and extrapath (may be NULL).
 * Also fills joins subpath and extrapath into dest
 *
 * If it exists and accessible (access is set by mask) returns B_TRUE.
 * Otherwise, returns B_FALSE.
 */
static boolean_t set_path(char* dest, size_t len, const char* root, const char* subpath,
						  const char* extrapath, int mask) {
	char path[PATHMAXLEN];

	if(subpath == NULL)
		return B_FALSE;

	path_join(path, PATHMAXLEN, root, subpath, extrapath, NULL);
	path_join(dest, len, subpath, extrapath, NULL);

	if(access(path, mask) == 0) {
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

	if(! (set_path(lib_path, PATHPARTMAXLEN, root,
				   INSTALL_LIB, NULL, R_OK | X_OK) ||
		  set_path(lib_path, PATHPARTMAXLEN, root,
				   getenv("TS_INSTALL_LIB"), NULL, R_OK | X_OK) ) ) {
		fprintf(stderr, "Cannot access lib path '%s', install core files "
				"or set TS_INSTALL_LIB\n", devel_path);
		exit(1);
	}

	if(! (set_path(mod_load_path, PATHPARTMAXLEN, root,
				   INSTALL_MOD_LOAD, NULL, R_OK | X_OK) ||
		  set_path(mod_load_path, PATHPARTMAXLEN, root,
				   getenv("TS_INSTALL_MOD_LOAD"), NULL, R_OK | X_OK) ) ) {
		fprintf(stderr, "Cannot access module install path '%s', install modules "
				"or set TS_INSTALL_MOD_LOAD\n", devel_path);
		exit(1);
	}

	if(! set_path(bldenv_path, PATHPARTMAXLEN, root,
				  devel_path, BUILDENV_FN, R_OK)) {
		fprintf(stderr, "Cannot access build environment file '%s', install devel files\n", bldenv_path);
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

/**
 * Preprocess input file identified by filename `fname` and write results to file `outf`
 */
int tsgenmodsrc_pp(const char* fname, FILE* outf) {
	char path[PATHMAXLEN];
	FILE* inf;
	int err = MODPP_OK;

	path_join(path, PATHMAXLEN, root_path, devel_path, MODSRC_DIR,
			  fname, NULL);

	inf = fopen(path, "r");

	if(inf == NULL) {
		fprintf(stderr, "Cannot open template file '%s'", path);
		return 1;
	}

	err = modpp_subst(inf, outf);
	if(err != MODPP_OK) {
		fprintf(stderr, "Failed to preprocess file '%s'", path);
		err = 1;
	}

	fclose(inf);

	return err;
}

/**
 * Create destination path for target `target`
 */
char* tsgenmodsrc_path_join(char* path, size_t len, modtarget_t* target) {
	const char* parts[MODTARGETPARTS];
	const char* part = NULL;
	int num_parts = 0;

	while(target->dst[num_parts] != NULL) {
		/* Resolve variables and build parts array */
		part = target->dst[num_parts];
		if(*part == '@') {
			++part;
			parts[num_parts] = modvar_get_string(part);
		}
		else {
			parts[num_parts] = part;
		}

		++num_parts;
	}

	return path_join_array(path, len, num_parts, parts);
}

/**
 * Walk over target list `tgtlist`. Calls `func` with argument `arg` for each destination file
 */
modtarget_t* tsgenmodsrc_walk(modtarget_t* tgtlist, modsrc_generate_walk_func func, void* arg) {
	char path[PATHMAXLEN];
	path_split_iter_t path_iter;
	const char* dirname;

	modtarget_t* target = tgtlist;

	int ret;

	while(target->dst[0] != NULL) {
		tsgenmodsrc_path_join(path, PATHMAXLEN, target);
		dirname = path_dirname(&path_iter, path);

		if(dirname)

		if(func(target, path, dirname, arg) != 0)
			return target;

		++target;
	}

	return NULL;
}

/**
 * Add file `tgt` to creation time cache. Creation time cache file
 * is used to track file changes outside of
 */
void ctime_cache_set(modtarget_t* tgt, const char* path) {
	struct stat st;
	char ctc_path[PATHMAXLEN];
	FILE* ctc_file;

	if(stat(path, &st) == -1) {
		/* File was not created, wat? */
		return;
	}

	path_join(ctc_path, PATHMAXLEN, modvar_get_string("MODINFODIR"),
			  CTIME_CACHE_FN, NULL);

	ctc_file = fopen(ctc_path, "a");
	if(ctc_file == NULL)
		return;

	fprintf(ctc_file, "%s %lld\n", tgt->src, (long long) st.st_ctime);

	fclose(ctc_file);
}

time_t ctime_cache_get(modtarget_t* tgt, const char* path) {
	char ctc_path[PATHMAXLEN];
	char line[256];
	long long t;
	size_t len;

	FILE* ctc_file;

	path_join(ctc_path, PATHMAXLEN, modvar_get_string("MODINFODIR"),
			  CTIME_CACHE_FN, NULL);

	ctc_file = fopen(ctc_path, "r");
	if(ctc_file == NULL)
		return (time_t) 0ll;

	while(!feof(ctc_file)) {
		if(fgets(line, 256, ctc_file) == NULL)
			break;

		len = strlen(tgt->src);
		if(strncmp(line, tgt->src, len) == 0) {
			sscanf(line + len, " %lld", &t);
			return (time_t) t;
		}
	}

	return (time_t) 0ll;
}

void ctime_cache_delete(void) {
	char ctc_path[PATHMAXLEN];

	path_join(ctc_path, PATHMAXLEN, modvar_get_string("MODINFODIR"),
			  CTIME_CACHE_FN, NULL);

	remove(ctc_path);
}

static int tsgenmodsrc_generate_walk(modtarget_t* tgt, const char* path, const char* dirname, void* arg) {
	struct stat st;
	FILE* outf;
	int ret;

	/* This is a path for cleanup, ignore it while generating files */
	if(tgt->src == NULL)
		return 0;

	fprintf(stderr, "Generating '%s'...\n", path);

	if(stat(dirname, &st) == -1) {
		if(mkdir(dirname, 0775) == -1) {
			fprintf(stderr, "Failed to create directory '%s': errno = %d\n", dirname, errno);
			return 1;
		}

		fprintf(stderr, "Created directory '%s'...\n", dirname);
	}

	outf = fopen(path, "w");
	if(outf == NULL) {
		fprintf(stderr, "Failed to open file '%s': errno = %d\n", path, errno);
		return 1;
	}

	ret = tsgenmodsrc_pp(tgt->src, outf);

	fclose(outf);

	/* Write ctime to ctime cache file */
	ctime_cache_set(tgt, path);

	return ret;
}

int tsgenmodsrc_generate(modtarget_t* tgtlist) {
	modtarget_t* target = tsgenmodsrc_walk(tgtlist, tsgenmodsrc_generate_walk, NULL);

	if(target == NULL)
		return 0;

	return 1;
}

static int tsgenmodsrc_clean_check_walk(modtarget_t* tgt, const char* path, const char* dirname, void* arg) {
	/* Check if file exists and mtime != ctime (so file was not modified) */
	struct stat st;
	time_t ctime;
	boolean_t* has_warnings_p = (boolean_t*) arg;

	if(tgt->src == NULL)
		return 0;

	if(stat(path, &st) == -1) {
		return 0;
	}

	ctime = ctime_cache_get(tgt, path);

	if(ctime == 0) {
		fprintf(stderr, "NOTICE: File '%s' is not tracked! \n", path);
		*has_warnings_p = B_TRUE;
		return 0;
	}

	if(ctime != st.st_mtime) {
		fprintf(stderr, "WARNING: File '%s' was modified! \n", path);
		*has_warnings_p = B_TRUE;
		return 0;
	}

	return 0;
}

static int tsgenmodsrc_clean_walk(modtarget_t* tgt, const char* path, const char* dirname, void* arg) {
	struct stat st;
	path_split_iter_t path_iter;
	const char* modinfo_dir = modvar_get_string("MODINFODIR");

	if(stat(path, &st) != -1) {
		if(remove(path) == -1) {
			fprintf(stderr, "Failed to clean file '%s', errno = %d. \n", path, errno);
			return 0;
		}

		fprintf(stderr, "Cleaned up file '%s'. \n", path);
	}

	while(strcmp(dirname, modinfo_dir) != 0 && remove(dirname) != -1) {
		fprintf(stderr, "Cleaned up directory '%s'. \n", dirname);

		dirname = path_dirname(&path_iter, dirname);
	}

	return 0;
}

int tsgenmodsrc_clean(void) {
	modtarget_t* tgtlist[] = { tgt_scons_int, tgt_scons_ext, tgt_make, tgt_build };
	int tli;
	char user_response;
	char expected_response;
	boolean_t has_warnings = B_FALSE;

	/* Prepare: check that files was not modified */
	for(tli = 0; tli < 4; ++tli) {
		if(tsgenmodsrc_walk(tgtlist[tli], tsgenmodsrc_clean_check_walk, &has_warnings) != NULL)
			return 1;
	}

	/* If warnings occur, let user press 'f', so if he presses 'y', he
	 * could think more time about what he is doing */
	expected_response = (has_warnings)? 'f' : 'y';

	do {
		fprintf(stderr, "Are you sure to clean [%c/n/h]? ", expected_response);
		scanf(" %c", &user_response);
		user_response = tolower(user_response);

		if(user_response == 'n')
			return 0;
		if(user_response == 'h') {
			fputs("\ty - Yes, clean\n"
				  "\tn - No, abort\n"
				  "\tf - Clean ignoring warnings\n",
				  stderr);
		}
	} while(user_response != expected_response);

	for(tli = 0; tli < 4; ++tli) {
		tsgenmodsrc_walk(tgtlist[tli], tsgenmodsrc_clean_walk, NULL);
	}

	ctime_cache_delete();

	return 0;
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
	init();

	modvar_set(modvar_create("TSLOADPATH"), root_path);
	modvar_set(modvar_create("TS_INSTALL_INCLUDE"), include_path);
	modvar_set(modvar_create("TS_INSTALL_DEVEL"), devel_path);
	modvar_set(modvar_create("TS_INSTALL_LIB"), lib_path);
	modvar_set(modvar_create("TS_INSTALL_MOD_LOAD"), mod_load_path);
		
	err =  modinfo_check_dir(modinfo_dir, !(command == MODSRC_GENERATE));
	if(command == MODSRC_GENERATE && err != 0) {
		return 1;
	}

	if(modinfo_read_buildenv(root_path, bldenv_path) != 0) {
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
		return tsgenmodsrc_pp(HEADER_IN_FN, stdout);
	case MODSRC_SHOW_SOURCE:
		return tsgenmodsrc_pp(SOURCE_IN_FN, stdout);
	case MODSRC_SHOW_MAKEFILE:
		if(build_system == MOD_BUILD_SCONS) {
			if(module_type == MOD_INTERNAL) {
				return tsgenmodsrc_pp(SCONSCRIPT_IN_FN, stdout);
			}
			else if(module_type == MOD_EXTERNAL) {
				return tsgenmodsrc_pp(SCONSTRUCT_IN_FN, stdout);
			}
		}
		else {
			return tsgenmodsrc_pp(MAKEFILE_IN_FN, stdout);
		}
		break;
	case MODSRC_GENERATE:
		if(build_system == MOD_BUILD_SCONS) {
			if(module_type == MOD_INTERNAL) {
				return tsgenmodsrc_generate(tgt_scons_int);
			}
			else if(module_type == MOD_EXTERNAL) {
				return tsgenmodsrc_generate(tgt_scons_ext);
			}
		}
		else {
			return tsgenmodsrc_generate(tgt_make);
		}
		break;
	case MODSRC_CLEAN:
		return tsgenmodsrc_clean();
	}

	return 0;
}
