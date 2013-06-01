/*
 * tstime.c
 *
 *  Created on: 22.12.2012
 *      Author: myaut
 */

#include <defs.h>
#include <tstime.h>

#include <time.h>

#define	GET_CLOCK(getter, CLOCK)	\
	struct timespec ts;				\
	ts_time_t t;					\
									\
	getter(CLOCK, &ts);				\
									\
	t = ts.tv_nsec;					\
	t += ts.tv_sec * T_SEC;			\
									\
	return t;

PLATAPI ts_time_t tm_get_time() {
	GET_CLOCK(clock_gettime, CLOCK_REALTIME);
}

PLATAPI ts_time_t tm_get_clock() {
	GET_CLOCK(clock_gettime, CLOCK_MONOTONIC);
}

PLATAPI ts_time_t tm_get_clock_res() {
	GET_CLOCK(clock_getres, CLOCK_MONOTONIC);
}
