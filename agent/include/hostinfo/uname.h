
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



#ifndef UNAME_H_
#define UNAME_H_

#include <tsload/defs.h>


/**
 * @module UName 
 * 
 * Get information on hardware platform and operating system agent is running on.
 * 
 * Strings that are returned by uname functions are statically allocated in
 * libhostinfo, no need to dispose them
 * 
 * These functions are optional, and may return generic data is they are not 
 * implemented on target platform
 */

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

