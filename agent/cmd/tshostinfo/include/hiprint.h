
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



#ifndef HIPRINT_H_
#define HIPRINT_H_

#include <defs.h>
#include <stdio.h>

#define INFO_DEFAULT	0
#define INFO_ALL		0x1
#define INFO_XDATA		0x2
#define INFO_LEGEND		0x4
#define INFO_JSON		0x8

#define INFO_OK		0
#define INFO_ERROR	1

int print_cpu_info(int flags);
int print_host_info(int flags);
int print_disk_info(int flags);
int print_sched_info(int flags);
int print_vm_info(int flags);
int print_net_info(int flags);

STATIC_INLINE void print_header(int flags, const char* header) {
	if(flags & INFO_ALL) {
		puts("");
		puts(header);
	}
}

#endif /* HIPRINT_H_ */

