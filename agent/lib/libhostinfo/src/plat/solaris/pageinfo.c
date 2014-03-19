/*
 * pageinfo.c
 *
 *  Created on: Mar 15, 2014
 *      Author: myaut
 */

#include <pageinfo.h>

#include <string.h>

#include <unistd.h>
#include <sys/mman.h>

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
