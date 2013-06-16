/*
 * linux_sysfs_bitmap.c
 *
 *  Created on: 02.05.2013
 *      Author: myaut
 */

#include <hitrace.h>

#include <assert.h>

extern int hi_linux_sysfs_parsebitmap(const char* str, uint32_t* bitmap, int len);

int test_main() {
	uint32_t bitmap;

#ifdef HOSTINFO_TRACE
	hi_trace_flags |= HI_TRACE_SYSFS;
#endif

	hi_linux_sysfs_parsebitmap("1", &bitmap, 1);
	assert(bitmap == 0x2);

	hi_linux_sysfs_parsebitmap("0-7", &bitmap, 1);
	assert(bitmap == 0xff);

	hi_linux_sysfs_parsebitmap("0-7,9", &bitmap, 1);
	assert(bitmap == 0x2ff);

	return 0;
}
