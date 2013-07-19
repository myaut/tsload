/*
 * log.c
 *
 *  Created on: 24.10.2012
 *      Author: myaut
 */

#include <defs.h>
#include <log.h>

#include <libjson.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <threads.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

LIBEXPORT char log_filename[LOGFNMAXLEN];

/* By default, debug and tracing are disabled
 * */
LIBEXPORT int log_debug = 0;
LIBEXPORT int log_trace	= 0;

boolean_t log_initialized = B_FALSE;

FILE* log_file;

/* Allows to dump backtraces with messages */
boolean_t log_trace_callers = B_FALSE;

thread_mutex_t	log_mutex;

const char* log_severity[] =
	{"CRIT", "ERR", "WARN", "INFO", "_DBG", "_TRC" };

#ifdef JSON_DEBUG
static void json_error_callback(const char* msg) {
	logmsg_src(LOG_DEBUG, "JSON", msg);
}
#endif

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

	if(strcmp(log_filename, "-") == 0) {
		log_file = stdout;
		return 0;
	}

	if((ret = log_rotate()) != 0)
		return ret;

	log_file = fopen(log_filename, "a");

	if(!log_file) {
		fprintf(stderr, "Couldn't open log file '%s'\n", log_filename);
		return -1;
	}

#	ifdef JSON_DEBUG
	json_register_debug_callback(json_error_callback);
#	endif

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
int logmsg_src(int severity, const char* source, const char* format, ...)
	{
	char time[64];
	va_list args;
	int ret = 0;

	if(!log_initialized)
		return -1;

	if((severity == LOG_DEBUG && log_debug == 0) ||
	   (severity == LOG_TRACE && log_trace == 0) ||
	    severity > LOG_TRACE || severity < 0)
			return -1;


	log_gettime(time, 64);

	mutex_lock(&log_mutex);

	ret = fprintf(log_file, "%s [%s:%4s] ", time, source, log_severity[severity]);

	va_start(args, format);
	ret += vfprintf(log_file, format, args);
	va_end(args);

	if(log_trace_callers) {
		char* callers = malloc(512);
		plat_get_callers(callers, 512);

		fputs(callers, log_file);
		free(callers);
	}

	fputc('\n', log_file);
	fflush(log_file);

	mutex_unlock(&log_mutex);

	return ret + 1;	/*+1 for \n*/
}

/* Log error provided by errno
 * */
int logerror() {
	return logmsg_src(LOG_CRIT, "error", "Error: %s", strerror(errno));
}
