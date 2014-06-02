
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

