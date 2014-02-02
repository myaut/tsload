/*
 * hiprint.h
 *
 *  Created on: 27.04.2013
 *      Author: myaut
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

STATIC_INLINE void print_header(int flags, const char* header) {
	if(flags & INFO_ALL) {
		puts("");
		puts(header);
	}
}

#endif /* HIPRINT_H_ */
