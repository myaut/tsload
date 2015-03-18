
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

#include <string.h>

#include <unistd.h>
#include <sys/mman.h>

/**
 * ### Solaris
 * 
 * Uses getpagesizes() and getpagesize() libc calls.
 */

#define PAGEINFOLEN					8

boolean_t hi_sol_pageinfo_probed = B_FALSE;

hi_page_info_t hi_sol_pageinfo[PAGEINFOLEN];

PLATAPI hi_page_info_t* hi_get_pageinfo(void) {
	int i, n;
	size_t default_pgsz;
	size_t pgsz[PAGEINFOLEN];

	if(hi_sol_pageinfo_probed)
		return hi_sol_pageinfo;

	memset(hi_sol_pageinfo, '\0', PAGEINFOLEN * sizeof(hi_page_info_t));

	default_pgsz = getpagesize();
	n = getpagesizes(pgsz, PAGEINFOLEN - 1);

	for(i = 0; i < n; ++i) {
		hi_sol_pageinfo[i].pi_flags = HI_PIF_PAGEINFO;

		if(pgsz[i] == default_pgsz) {
			hi_sol_pageinfo[i].pi_flags |= HI_PIF_DEFAULT;
		}

		hi_sol_pageinfo[i].pi_size = pgsz[i];
	}

	return hi_sol_pageinfo;
}

