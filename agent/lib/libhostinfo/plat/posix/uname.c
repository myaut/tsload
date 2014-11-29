
/*
    This file is part of TSLoad.
    Copyright 2012-2013, Sergey Klyaus, ITMO University

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

#include <hostinfo/uname.h>

#include <sys/utsname.h>


static boolean_t uname_read = B_FALSE;
static struct utsname uname_data;

struct utsname* hi_get_uname() {
	if(!uname_read) {
		uname(&uname_data);
		uname_read = B_TRUE;
	}

	return &uname_data;
}

/* Returns operating system name and version */
PLATAPI const char* hi_get_os_name() {
	return hi_get_uname()->sysname;
}

PLATAPI const char* hi_get_os_release() {
	return hi_get_uname()->release;
}

/* Returns nodename and domain name of current host*/
PLATAPI const char* hi_get_nodename() {
	return hi_get_uname()->nodename;
}

/* Returns machine architecture */
PLATAPI const char* hi_get_mach() {
	return hi_get_uname()->machine;
}


