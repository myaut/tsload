/*
 * threads.c
 *
 *  Created on: 23.12.2012
 *      Author: myaut
 */

#include <defs.h>

#include <unistd.h>
#include <sys/syscall.h>

PLATAPI unsigned long plat_gettid() {
#ifdef HAVE_DECL___NR_GETTID
# 	include <sys/syscall.h>
	return syscall(__NR_gettid);
#else
	return 0;
#endif
}
