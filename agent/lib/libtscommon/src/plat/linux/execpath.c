/*
 * execpath.c
 *
 *  Created on: Jan 13, 2014
 *      Author: myaut
 */

#include <execpath.h>
#include <pathutil.h>
#include <readlink.h>

PLATAPIDECL(plat_execpath) char cur_execpath[PATHMAXLEN];
PLATAPIDECL(plat_execpath) boolean_t have_execpath = B_FALSE;

PLATAPI const char* plat_execpath(void) {
	if(have_execpath)
		return cur_execpath;

	if(plat_readlink("/proc/self/exe", cur_execpath, PATHMAXLEN) == -1)
		return NULL;

	have_execpath = B_TRUE;
	return cur_execpath;
}
