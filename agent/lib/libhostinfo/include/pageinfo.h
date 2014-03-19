/*
 * pageinfo.h
 *
 *  Created on: Mar 15, 2014
 *      Author: myaut
 */

#ifndef PAGEINFO_H_
#define PAGEINFO_H_

#include <defs.h>

#define HI_PIF_PAGEINFO			0x01
#define	HI_PIF_TLBINFO			0x02
#define HI_PIF_DEFAULT			0x04
#define HI_PIF_HUGEPAGE			0x08

typedef struct hi_page_info {
	int    pi_flags;
	size_t pi_size;
	int    pi_itlb_entries;
	int    pi_dtlb_entries;
} hi_page_info_t;

LIBEXPORT PLATAPI hi_page_info_t* hi_get_pageinfo(void);

#endif /* PAGEINFO_H_ */
