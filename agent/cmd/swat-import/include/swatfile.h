
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



#ifndef SWATFILE_H_
#define SWATFILE_H_

#include <tsload/defs.h>


#define SWAT_FLAT_RECORD	5
#define SWAT_LONG_SIZE		8

#define SWAT_OK					0
#define SWAT_ERR_OPEN_FILE		-1

/* From http://trac.aircrack-ng.org/svn/trunk/src/osdep/byteorder.h */
#define SWAT_32_ENDIANESS(x) \
	((u_int32_t)( \
			(((u_int32_t)(x) & (u_int32_t)0x000000ffUL) << 24) | \
			(((u_int32_t)(x) & (u_int32_t)0x0000ff00UL) <<  8) | \
			(((u_int32_t)(x) & (u_int32_t)0x00ff0000UL) >>  8) | \
			(((u_int32_t)(x) & (u_int32_t)0xff000000UL) >> 24) ))

#define SWAT_64_ENDIANESS(x) \
((u_int64_t)( \
		(u_int64_t)(((u_int64_t)(x) & (u_int64_t)0x00000000000000ffULL) << 56) | \
		(u_int64_t)(((u_int64_t)(x) & (u_int64_t)0x000000000000ff00ULL) << 40) | \
		(u_int64_t)(((u_int64_t)(x) & (u_int64_t)0x0000000000ff0000ULL) << 24) | \
		(u_int64_t)(((u_int64_t)(x) & (u_int64_t)0x00000000ff000000ULL) <<  8) | \
		(u_int64_t)(((u_int64_t)(x) & (u_int64_t)0x000000ff00000000ULL) >>  8) | \
		(u_int64_t)(((u_int64_t)(x) & (u_int64_t)0x0000ff0000000000ULL) >> 24) | \
		(u_int64_t)(((u_int64_t)(x) & (u_int64_t)0x00ff000000000000ULL) >> 40) | \
		(u_int64_t)(((u_int64_t)(x) & (u_int64_t)0xff00000000000000ULL) >> 56) ))

int swat_read(boolean_t do_report);

#endif /* SWATFILE_H_ */

