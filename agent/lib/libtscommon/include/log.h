/*
 * log.h
 *
 *  Created on: 24.10.2012
 *      Author: myaut
 */

#ifndef LOG_H_
#define LOG_H_

#include <defs.h>

/**
 * @module Logging facility
 */

#ifndef LOG_SOURCE
#define LOG_SOURCE ""
#endif

#define LOGFNMAXLEN 	128
#define LOGMAXSIZE 		2 * 1024 * 1024
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

LIBEXPORT int logmsg_src(int severity, const char* source, const char* format, ...)
		CHECKFORMAT(printf, 3, 4);		/*For GCC printf warnings*/;

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

#endif /* LOG_H_ */
