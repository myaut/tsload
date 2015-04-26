/*
 * main.c
 *
 *  Created on: Sep 1, 2014
 *      Author: myaut
 */

#include <tsload/log.h>

#include <tsload/defs.h>
#include <tsload/posixdecl.h>

#include <tsload/pathutil.h>
#include <tsload/init.h>

#include <hostinfo/hiobject.h>

#include <tsload.h>

extern int tsload_test_main();

LIBIMPORT char log_filename[];
LIBIMPORT char mod_search_path[];

int init(void);

void tse_error_msg(ts_errcode_t code, const char* format, ...) {
	va_list va;
	char fmtstr[256];

	snprintf(fmtstr, 256, "ERROR %d: %s\n", code, format);

	va_start(va, format);
	vfprintf(stderr, fmtstr, va);
	va_end(va);
}

int test_main(int argc, char* argv[]) {
	int err = 0;
	char cwd[PATHMAXLEN];

	strcpy(log_filename, "-");

	getcwd(cwd, PATHMAXLEN);
	path_join(mod_search_path, MODPATHLEN, cwd, "mod", NULL);
	
	strncpy(hi_obj_modpath, getenv("TS_HIMODPATH"), PATHMAXLEN);

	init();

	tsload_error_msg = tse_error_msg;

	return tsload_test_main();
}
