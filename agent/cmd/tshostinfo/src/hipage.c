/*
 * hipage.c
 *
 *  Created on: Mar 15, 2014
 *      Author: myaut
 */

#include <pageinfo.h>

#include <hiprint.h>

int print_vm_info(int flags) {
	hi_page_info_t* page_info;
	int polid = 0;

	print_header(flags, "VM INFORMATION");

	page_info = hi_get_pageinfo();

	if(flags & INFO_LEGEND)
		printf("  %16s %8s %8s\n", "PAGESIZE", "iTLB", "dTLB");

	while(page_info->pi_flags != 0) {
		if(page_info->pi_flags & HI_PIF_DEFAULT) {
			fputc('*', stdout);
		}
		else if(page_info->pi_flags & HI_PIF_HUGEPAGE) {
			fputc('H', stdout);
		}
		else {
			fputc(' ', stdout);
		}

		if(page_info->pi_flags & HI_PIF_PAGEINFO) {
			printf(" %16llu", (unsigned long long) page_info->pi_size);
		}
		else {
			printf(" %16s", "?");
		}

		if(page_info->pi_flags & HI_PIF_TLBINFO) {
			printf(" %8d %8d\n", page_info->pi_itlb_entries, page_info->pi_dtlb_entries);
		}
		else {
			printf(" %8s %8s\n", "?", "?");
		}

		++page_info;
	}

	return INFO_OK;
}
