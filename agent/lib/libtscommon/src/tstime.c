/*
 * time.c
 *
 *  Created on: 06.12.2012
 *      Author: myaut
 */

#include <defs.h>
#include <tstime.h>

#include <stdlib.h>

ts_time_t tm_diff(ts_time_t a, ts_time_t b) {
	return b - a;
}

/**
 * Ceils time tm with precision and returns difference between ceiled
 * value and original tm.
 * */
ts_time_t tm_ceil_diff(ts_time_t tm, ts_time_t precision) {
	/* Let mod = tm % precision
	 *
	 * ceiled value = | tm if mod == 0
	 *                | tm + (precision - mod) if mod > 0
	 *
	 * So diff = | 0 if mod == 0
	 *           | precision - mod if mod > 0*/

	ts_time_t mod = tm % precision;

	return (mod == 0)? 0 : (precision - mod);
}
