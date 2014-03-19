/*
 * hitrace.h
 *
 *  Created on: 02.05.2013
 *      Author: myaut
 */

#ifndef HITRACE_H_
#define HITRACE_H_

#include <defs.h>

#define HI_TRACE_UNAME		0x1
#define HI_TRACE_DSK		0x2
#define HI_TRACE_CPU		0x4
#define HI_TRACE_OBJ		0x8

#ifdef PLAT_LINUX
#define HI_TRACE_SYSFS		0x100
#endif

#ifdef PLAT_WIN
#define HI_TRACE_WMI		0x100
#endif

#ifdef HOSTINFO_TRACE
extern int hi_trace_flags;

#include <stdio.h>
#define hi_trace_dprintf( flags, ... )	\
			if(hi_trace_flags & flags) printf(  __VA_ARGS__ )
#else

#define hi_trace_dprintf( flags, ... )
#endif

#define hi_uname_dprintf( ... ) 	hi_trace_dprintf(HI_TRACE_UNAME, __VA_ARGS__ )
#define hi_dsk_dprintf( ... )		hi_trace_dprintf(HI_TRACE_DSK, __VA_ARGS__ )
#define hi_cpu_dprintf( ... ) 		hi_trace_dprintf(HI_TRACE_CPU, __VA_ARGS__ )
#define hi_obj_dprintf( ... ) 		hi_trace_dprintf(HI_TRACE_OBJ, __VA_ARGS__ )

#ifdef PLAT_LINUX
#define hi_sysfs_dprintf( ... ) hi_trace_dprintf(HI_TRACE_SYSFS, __VA_ARGS__ )
#endif

#ifdef PLAT_WIN
#define hi_wmi_dprintf( ... ) hi_trace_dprintf(HI_TRACE_WMI, __VA_ARGS__ )
#endif


#endif /* HITRACE_H_ */
