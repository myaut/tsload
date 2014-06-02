
/*
    This file is part of TSLoad.
    Copyright 2013, Sergey Klyaus, ITMO University

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

