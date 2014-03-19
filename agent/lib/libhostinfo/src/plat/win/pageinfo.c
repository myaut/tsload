#include <pageinfo.h>

#include <stdio.h>
#include <string.h>

#include <windows.h>

boolean_t hi_win_pageinfo_probed = B_FALSE;
hi_page_info_t hi_win_pageinfo[3];

PLATAPI hi_page_info_t* hi_get_pageinfo(void) {
	SYSTEM_INFO sysinfo;

	if(hi_win_pageinfo_probed) {
		return hi_win_pageinfo;
	}

	GetSystemInfo(&sysinfo);

	hi_win_pageinfo[0].pi_flags = HI_PIF_DEFAULT | HI_PIF_PAGEINFO;
	hi_win_pageinfo[0].pi_size = sysinfo.dwPageSize;

	hi_win_pageinfo[1].pi_flags = HI_PIF_HUGEPAGE | HI_PIF_PAGEINFO;
	hi_win_pageinfo[1].pi_size = GetLargePageMinimum();

	hi_win_pageinfo[2].pi_flags = 0;

	hi_win_pageinfo_probed = B_TRUE;
	return hi_win_pageinfo;
}
