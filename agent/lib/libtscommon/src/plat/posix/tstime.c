/*
 * tstime.c
 *
 *  Created on: 22.12.2012
 *      Author: myaut
 */

#include <defs.h>
#include <tstime.h>

#include <unistd.h>
#include <sys/time.h>

PLATAPI ts_time_t tm_get_time() {
	struct timeval tv;
	gettimeofday(&tv, NULL);

	return tv.tv_usec * T_US + tv.tv_sec * T_SEC;
}

PLATAPI void tm_sleep_milli(ts_time_t t) {
	usleep(t / T_US);
}

PLATAPI void tm_sleep_nano(ts_time_t t) {
	struct timespec ts;
	struct timespec rem;

	int ret = -1;

	ts.tv_sec = t / T_SEC;
	ts.tv_nsec = t % T_SEC;

	while(ret != 0) {
		ret = nanosleep(&ts, &rem);

		/* Interrupted by signal, try again */
		ts.tv_sec = rem.tv_sec;
		ts.tv_nsec = rem.tv_nsec;
	}
}
