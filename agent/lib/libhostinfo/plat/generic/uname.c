
/*
    This file is part of TSLoad.
    Copyright 2012-2014, Sergey Klyaus, ITMO University

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


PLATAPIDECL(hi_get_nodename) char nodename[64];
PLATAPIDECL(hi_get_mach) char mach[64];

/* Returns operating system name and version */
PLATAPI const char* hi_get_os_name() {
	return "Generic OS";
}

PLATAPI const char* hi_get_os_release() {
	return "1.0";
}

/* Returns nodename and domain name of current host*/
PLATAPI const char* hi_get_nodename() {
	/*NOTE: hostname should be set via config file*/
	return nodename;
}

PLATAPI const char* hi_get_domainname() {
	return "";
}

/* Returns machine architecture */
PLATAPI const char* hi_get_mach() {
	return mach;
}

/* Returns system model and vendor */
PLATAPI const char* hi_get_sys_name() {
	return "Unknown";
}

