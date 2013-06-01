/*
 * uname.c
 *
 *  Created on: Dec 10, 2012
 *      Author: myaut
 */

#include <defs.h>
#include <uname.h>

#include <windows.h>

LIBEXPORT PLATAPIDECL(hi_get_nodename) char nodename[MAX_COMPUTERNAME_LENGTH + 1];

/*TODO: implement uname for MS Windows*/

/* Returns operating system name and version */
PLATAPI const char* hi_get_os_name() {
	return "Windows";
}

PLATAPI const char* hi_get_os_release() {
	return "3.11";
}

/* Returns nodename and domain name of current host*/
PLATAPI const char* hi_get_nodename() {
	DWORD size = 64;
	GetComputerName(nodename, &size);

	return nodename;
}

PLATAPI const char* hi_get_domainname() {
	return "";
}

/* Returns machine architecture */
PLATAPI const char* hi_get_mach() {
	return "?";
}

