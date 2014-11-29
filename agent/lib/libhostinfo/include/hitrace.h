
/*
    This file is part of TSLoad.
    Copyright 2013-2014, Sergey Klyaus, ITMO University

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



#ifndef HITRACE_H_
#define HITRACE_H_

#include <tsload/defs.h>


/**
 * HostInfo tracing flags
 */
#define HI_TRACE_UNAME		0x1
#define HI_TRACE_DSK		0x2
#define HI_TRACE_CPU		0x4
#define HI_TRACE_OBJ		0x8
#define HI_TRACE_NET		0x10

#ifdef PLAT_LINUX
#define HI_TRACE_SYSFS		0x100
#endif

#ifdef PLAT_WIN
#define HI_TRACE_WMI		0x100
#endif

#ifdef HOSTINFO_TRACE

LIBVARIABLE unsigned hi_trace_flags;

#include <stdio.h>
#define hi_trace_dprintf( flags, ... )	\
			if(hi_trace_flags & flags) fprintf(stderr,  __VA_ARGS__ )


#else

#define hi_trace_dprintf( flags, ... )
#endif

#define hi_uname_dprintf( ... ) 	hi_trace_dprintf(HI_TRACE_UNAME, __VA_ARGS__ )
#define hi_dsk_dprintf( ... )		hi_trace_dprintf(HI_TRACE_DSK, __VA_ARGS__ )
#define hi_cpu_dprintf( ... ) 		hi_trace_dprintf(HI_TRACE_CPU, __VA_ARGS__ )
#define hi_obj_dprintf( ... ) 		hi_trace_dprintf(HI_TRACE_OBJ, __VA_ARGS__ )
#define hi_net_dprintf( ... ) 		hi_trace_dprintf(HI_TRACE_NET, __VA_ARGS__ )

#ifdef PLAT_LINUX
#define hi_sysfs_dprintf( ... ) hi_trace_dprintf(HI_TRACE_SYSFS, __VA_ARGS__ )
#endif

#ifdef PLAT_WIN
#define hi_wmi_dprintf( ... ) hi_trace_dprintf(HI_TRACE_WMI, __VA_ARGS__ )
#endif


#endif /* HITRACE_H_ */

