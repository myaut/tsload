/*
 * uname.c
 *
 *  Created on: Dec 17, 2012
 *      Author: myaut
 */

#include <uname.h>

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
