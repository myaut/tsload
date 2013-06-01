/*
 * hihost.c
 *
 *  Created on: 27.04.2013
 *      Author: myaut
 */

#include <hiprint.h>
#include <uname.h>

#include <stdio.h>

int print_host_info(int flags) {
	print_header(flags, "HOST INFORMATION");

	if(flags & (INFO_LEGEND | INFO_XDATA)) {
		printf("%-16s: %s\n", "nodename", hi_get_nodename());
		printf("%-16s: %s\n", "domain", hi_get_domainname());
		printf("%-16s: %s\n", "osname", hi_get_os_name());
		printf("%-16s: %s\n", "release", hi_get_os_release());
		printf("%-16s: %s\n", "mach", hi_get_mach());
	}
	else {
		printf("%s %s.%s %s %s\n", hi_get_os_name(),
				hi_get_nodename(), hi_get_domainname(),
				hi_get_os_release(), hi_get_mach());
	}

	return INFO_OK;
}
