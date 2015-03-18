
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



#ifndef PAGEINFO_H_
#define PAGEINFO_H_

#include <tsload/defs.h>

/**
 * @module PageInfo
 * 
 * Gets allowed sizes of virtual memory pages
 */

/**
 * Page information flags that are saved into `pi_flags` field.
 * 
 * @value HI_PIF_PAGEINFO	this entry contains page information 
 * @value HI_PIF_TLBINFO	this entry contains TLB information
 * @value HI_PIF_DEFAULT 	this is default pagesize (minimum)
 * @value HI_PIF_HUGEPAGE	this page is considered "large"
 */
#define HI_PIF_PAGEINFO			0x01
#define	HI_PIF_TLBINFO			0x02
#define HI_PIF_DEFAULT			0x04
#define HI_PIF_HUGEPAGE			0x08

/**
 * Information about page supported by processor
 * 
 * @member pi_flags		flags (see above). If set to 0, this is last entry in array
 * @member pi_size		size of page in bytes
 * @member pi_itlb_entries number of instruction TLB entries
 * @member pi_dtlb_entries number of data TLB entries
 */
typedef struct hi_page_info {
	int    pi_flags;
	size_t pi_size;
	int    pi_itlb_entries;
	int    pi_dtlb_entries;
} hi_page_info_t;

/**
 * Get information on supported page sizes by platform
 * Returns array of `hi_page_info_t` structures
 * 
 * Availabilty of most information is platform-dependent, but 
 * this function guarantee to provide correct pagesize for `HI_PIF_DEFAULT` pageinfo
 * 
 * TLB information is not provided, but it may be provided by [CPUInfo][hostinfo/cpuinfo]
 * 
 * Returned array is statically allocated, no need to dispose it.
 */
LIBEXPORT PLATAPI hi_page_info_t* hi_get_pageinfo(void);

#endif /* PAGEINFO_H_ */

