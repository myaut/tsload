/*
 * execpath.c
 *
 *  Created on: Jan 13, 2014
 *      Author: myaut
 */

#include <execpath.h>
#include <pathutil.h>

#include <windows.h>

PLATAPIDECL(plat_execpath) char cur_execpath[PATHMAXLEN];
PLATAPIDECL(plat_execpath) boolean_t have_execpath = B_FALSE;

PLATAPI const char* plat_execpath(void) {
	DWORD ret;

	if(have_execpath)
		return cur_execpath;

	ret = GetModuleFileName(NULL, cur_execpath, PATHMAXLEN);

	if(ret == 0 || ret == PATHMAXLEN)
		return NULL;

	have_execpath = B_TRUE;
	return cur_execpath;
}
