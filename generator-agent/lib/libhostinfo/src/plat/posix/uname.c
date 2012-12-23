/*
 * uname.c
 *
 *  Created on: Dec 10, 2012
 *      Author: myaut
 */

#include <defs.h>
#include <uname.h>

#include <sys/utsname.h>

static int uname_read = B_FALSE;
static struct utsname uname_data;

static inline struct utsname* hi_get_uname() {
	if(!uname_read)
		uname(&uname_data);

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

PLATAPI const char* hi_get_domainname() {
	/*FIXME: solaris/bsd implementation*/
	return hi_get_uname()->__domainname;
}

/* Returns machine architecture */
PLATAPI const char* hi_get_mach() {
	return hi_get_uname()->machine;
}

