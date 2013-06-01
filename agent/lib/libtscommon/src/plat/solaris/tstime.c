/*
 * tstime.c
 *
 *  Created on: 22.12.2012
 *      Author: myaut
 */

#include <defs.h>
#include <tstime.h>

#include <sys/time.h>

PLATAPI ts_time_t tm_get_clock() {
	return gethrtime();
}

PLATAPI ts_time_t tm_get_clock_res() {
	/* FIXME: This is not hardware resolution of timer */
	return T_NS;
}
