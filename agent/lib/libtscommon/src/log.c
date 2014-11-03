
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



#include <defs.h>
#include <log.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <threads.h>
#include <tuneit.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

LIBEXPORT char log_filename[LOGFNMAXLEN];

/**
 * By default, debug and tracing are disabled
 * set log_debug or log_trace to 1 if you want to enable it
 * */
LIBEXPORT boolean_t log_debug = B_FALSE;
LIBEXPORT boolean_t log_trace = B_FALSE;

boolean_t log_initialized = B_FALSE;

FILE* log_file;

/**
 * Allows to dump backtraces with messages */
boolean_t log_trace_callers = B_FALSE;

thread_mutex_t	log_mutex;

const char* log_severity[] =
	{"CRIT", "ERR", "WARN", "INFO", "_DBG", "_TRC" };

/* Rotate tsload logs
 *
 * if logfile size is greater than LOGMAXSIZE (2M) by default,
 * it moves logfile to .old file*/
int log_rotate() {
	char old_log_filename[LOGFNMAXLEN + 4];
	struct stat log_stat;
	struct stat old_log_stat;

	if(stat(log_filename, &log_stat) == -1) {
		/* Log doesn't exist, so we will create new file */
		return 0;
	}

	if(log_stat.st_size < LOGMAXSIZE)
		return 0;

	strcpy(old_log_filename, log_filename);
	strcat(old_log_filename, ".old");

	if(stat(old_log_filename, &old_log_stat) == 0)
		remove(old_log_filename);

	rename(log_filename, old_log_filename);

	return 0;
}

int log_init() {
	int ret;

	mutex_init(&log_mutex, "log_mutex");

	if(getenv("TS_DEBUG") != NULL) {
		log_debug = B_TRUE;
	}
	if(getenv("TS_TRACE") != NULL) {
		log_debug = B_TRUE;
		log_trace = B_TRUE;
	}

	tuneit_set_bool(log_debug);
	tuneit_set_bool(log_trace);

	if(strcmp(log_filename, "-") == 0) {
		log_file = stderr;
	}
	else {
		if((ret = log_rotate()) != 0)
			return ret;

		log_file = fopen(log_filename, "a");

		if(!log_file) {
			fprintf(stderr, "Couldn't open log file '%s'\n", log_filename);
			return -1;
		}
	}

	log_initialized = B_TRUE;

	return 0;
}

void log_fini() {
	fclose(log_file);

	mutex_destroy(&log_mutex);
}

void log_gettime(char* buf, int sz) {
	time_t rawtime;
	struct tm* timeinfo;

	/* Non-reenterable, but protected by upper mutex (log_mutex) */
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );

	strftime(buf, sz, "%c", timeinfo);
}

/**
 * Log message to default logging location
 * No need to add \n to format string
 *
 * @param severity - level of logging (LOG_CRIT, LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG, LOG_TRACING)
 * @param source - origin of message
 * @param format - printf format
 *
 * @return number of written symbols or -1 if message was discarded due to severity
 */
int logmsg_src(int severity, const char* source, const char* format, ...) {
	int ret;
	va_list va;

	va_start(va, format);
	ret = logmsg_src_va(severity, source, format, va);
	va_end(va);

	return ret;
}

/**
 * Same as logmsg_src(), but accepts varargs
 */
int logmsg_src_va(int severity, const char* source, const char* format, va_list args)
{
	char time[64];
	int ret = 0;

	if(!log_initialized)
		return -1;

	if((severity == LOG_DEBUG && log_debug == 0) ||
	   (severity == LOG_TRACE && log_trace == 0) ||
	    severity > LOG_TRACE || severity < 0)
			return -1;

	mutex_lock(&log_mutex);

	log_gettime(time, 64);

	ret = fprintf(log_file, "%s [%s:%4s] ", time, source, log_severity[severity]);

	ret += vfprintf(log_file, format, args);

	if(log_trace_callers) {
		char* callers = malloc(512);
		plat_get_callers(callers, 512);

		fputs(callers, log_file);
		free(callers);
	}

	if(*(format + strlen(format) - 1) != '\n') {
		fputc('\n', log_file);
		ret += 1;
	}

	fflush(log_file);

	mutex_unlock(&log_mutex);

	return ret;
}

/**
 * Log error provided by errno
 * */
int logerror() {
	/* FIXME: Should be PLATAPI and have Windows implementation */
	return logmsg_src(LOG_CRIT, "error", "Error: %s", strerror(errno));
}

