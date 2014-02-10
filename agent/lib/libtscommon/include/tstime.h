/*
 * tstime.h
 *
 *  Created on: 06.12.2012
 *      Author: myaut
 */

#ifndef TS_TIME_H_
#define TS_TIME_H_

#include <defs.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>

/**
 * 64-bit integer is enough to keep time up to XXIIIth century
 * with nanosecond precision. I hope, that we will release patch until then.
 * */
typedef int64_t ts_time_t;

#define TS_TIME_MAX 	LLONG_MAX

#define TS_TIME_TO_UNIX(t)	((time_t) ((t) / T_SEC))

/* Get ts_time's component. as long I.e for 789.123456 s
 * it will return 789 secs (TS_TIME_SEC), 123 msecs and 456 us */
#define TS_TIME_SEC(t)		((long) (t / T_SEC))
#define TS_TIME_MS(t)		((long) ((t / T_MS) % (T_SEC / T_MS)))
#define TS_TIME_US(t)		((long) ((t / T_US) % (T_MS / T_US)))
#define TS_TIME_NS(t)		((long) (t % (T_US / T_NS)))

/**
 * Returns time since Epoch.
 * Guarantees millisecond precision */
LIBEXPORT PLATAPI ts_time_t tm_get_time();

/**
 * Returns clock (monotonic time since some unspecified starting point)
 * Guarantees 100-nanosecond precision (Windows) or better (Unix) */
LIBEXPORT PLATAPI ts_time_t tm_get_clock();

/**
 * Returns clock's resolution */
LIBEXPORT PLATAPI ts_time_t tm_get_clock_res();

LIBEXPORT ts_time_t tm_diff(ts_time_t a, ts_time_t b);

/**
 * Sleep with millisecond precision * */
LIBEXPORT PLATAPI void tm_sleep_milli(ts_time_t t);

/**
 * Sleep with nanosecond precision */
LIBEXPORT PLATAPI void tm_sleep_nano(ts_time_t t);

LIBEXPORT ts_time_t tm_ceil_diff(ts_time_t tm, ts_time_t precision);

LIBEXPORT size_t tm_human_print(ts_time_t t, const char* dst, size_t size);
LIBEXPORT size_t tm_datetime_print(ts_time_t t, const char* dst, size_t size);


#endif /* TS_TIME_H_ */
