
/*
    This file is part of TSLoad.
    Copyright 2012-2014, Sergey Klyaus, ITMO University

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



#ifndef LOG_H_
#define LOG_H_

#include <tsload/defs.h>

#include <stdarg.h>


/**
 * @module Logging facility
 */

#ifndef LOG_SOURCE
#define LOG_SOURCE ""
#endif

#define LOGMAXSIZE 			2 * 1024 * 1024
#define LOG_MAX_BACKTRACE	12

/**
 * @name Logging severities.
 *
 * LOG_DEBUG and LOG_TRACE are working only if appropriate variables are set.
 * */
#define LOG_CRIT	0
#define LOG_ERROR	1
#define LOG_WARN	2
#define LOG_INFO	3
#define LOG_DEBUG	4
#define LOG_TRACE	5

LIBEXPORT int log_init();
LIBEXPORT void log_fini();

LIBEXPORT int logerror();

LIBEXPORT int logmsg_src_va(int severity, const char* source, const char* format, va_list args);
LIBEXPORT int logmsg_src(int severity, const char* source, const char* format, ...)
	CHECKFORMAT(printf, 3, 4);		/*For GCC printf warnings*/

LIBEXPORT PLATAPI int plat_get_callers(char* callers, size_t size);

/**
 * Logging macro. Implemented by logmsg_src()
 *
 * To use it, define LOG_SOURCE before including log.h:
 * ```
 * #define LOG_SOURCE
 * #include <log.h>
 * ```
 * */
#define logmsg(severity, ...) \
	logmsg_src((severity), LOG_SOURCE, __VA_ARGS__)
#define logmsg_va(severity, format, va) \
	logmsg_src_va((severity), LOG_SOURCE, format, va)

#endif /* LOG_H_ */

