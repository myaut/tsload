/*
 * execpath.c
 *
 *  Created on: Jan 13, 2014
 *      Author: myaut
 */

#include <execpath.h>
#include <pathutil.h>

#include <stdlib.h>
#include <unistd.h>

PLATAPIDECL(plat_execpath) char cur_execpath[PATHMAXLEN];
PLATAPIDECL(plat_execpath) boolean_t have_execpath = B_FALSE;

PLATAPI const char* plat_execpath(void) {
	char cur_workdir[PATHMAXLEN];
	char* execname;

	if(have_execpath)
		return cur_execpath;

	if((execname = getexecname()) == NULL ||
	    (getcwd(cur_workdir, PATHMAXLEN) == NULL))
			return NULL;

	path_join(cur_execpath, PATHMAXLEN,
			  cur_workdir, execname, NULL);

	have_execpath = B_TRUE;
	return cur_execpath;
}
