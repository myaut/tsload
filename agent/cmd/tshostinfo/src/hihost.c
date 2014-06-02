
/*
    This file is part of TSLoad.
    Copyright 2013-2014, Sergey Klyaus, ITMO University

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
		printf("%-16s: %s\n", "system", hi_get_sys_name());
	}
	else {
		printf("%s %s.%s %s %s\n", hi_get_os_name(),
				hi_get_nodename(), hi_get_domainname(),
				hi_get_os_release(), hi_get_mach());
	}

	return INFO_OK;
}

