/*
 * uname.h
 *
 *  Created on: Dec 10, 2012
 *      Author: myaut
 */

#ifndef UNAME_H_
#define UNAME_H_

#include <defs.h>

/**
 * Returns operating system name and version */
LIBEXPORT PLATAPI const char* hi_get_os_name();
LIBEXPORT PLATAPI const char* hi_get_os_release();

/**
 * Returns nodename and domain name of current host*/
LIBEXPORT PLATAPI const char* hi_get_nodename();
LIBEXPORT PLATAPI const char* hi_get_domainname();

/**
 * Returns machine architecture */
LIBEXPORT PLATAPI const char* hi_get_mach();

/**
 * Returns system model and vendor */
LIBEXPORT PLATAPI const char* hi_get_sys_name();


#endif /* UNAME_H_ */
