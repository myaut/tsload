
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

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

#include <tsload/execpath.h>
#include <tsload/pathutil.h>
#include <tsload/readlink.h>


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

