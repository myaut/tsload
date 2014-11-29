
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

