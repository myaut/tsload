
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

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



#include <tsload/defs.h>

#include <hostinfo/pageinfo.h>
#include <hostinfo/plat/sysfs.h>

#include <stdio.h>
#include <string.h>

#include <unistd.h>


#define PAGEINFOLEN					8

#define SYS_HUGEPAGE_PATH 	"/sys/kernel/mm/hugepages/"

boolean_t hi_lnx_pageinfo_probed = B_FALSE;
hi_page_info_t hi_lnx_pageinfo[PAGEINFOLEN];

static void hi_linux_pi_hugeproc(const char* name, void* arg) {
	int* pi = (int*) arg;
	unsigned long long pagesize;

	/* No more pages in statically allocated array */
	if(*pi == (PAGEINFOLEN - 1))
		return;

	/* Failed to scan it! */
	if(sscanf(name, "hugepages-%llukB", &pagesize) != 1)
		return;

	hi_lnx_pageinfo[*pi].pi_flags = HI_PIF_PAGEINFO | HI_PIF_HUGEPAGE;
	hi_lnx_pageinfo[*pi].pi_size = pagesize * SZ_KB;

	++(*pi);
}

PLATAPI hi_page_info_t* hi_get_pageinfo(void) {
	int i;

	if(hi_lnx_pageinfo_probed)
		return hi_lnx_pageinfo;

	memset(hi_lnx_pageinfo, '\0', PAGEINFOLEN * sizeof(hi_page_info_t));

	/* Fill default page size */
	hi_lnx_pageinfo[0].pi_flags = HI_PIF_PAGEINFO | HI_PIF_DEFAULT;
	hi_lnx_pageinfo[0].pi_size = sysconf(_SC_PAGE_SIZE);

	i = 1;
	hi_linux_sysfs_walk(SYS_HUGEPAGE_PATH, hi_linux_pi_hugeproc, &i);

	return hi_lnx_pageinfo;
}

