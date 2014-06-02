
/*
    This file is part of TSLoad.
    Copyright 2012-2013, Sergey Klyaus, ITMO University

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

