
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
#include <time.h>
#include <tstime.h>

#include <stdlib.h>
#include <stdio.h>

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

struct tm_component {
	ts_time_t tm;
	const char* suffix;
};

/**
 * Prints seconds - nanoseconds in human-readable format
 * Recommended buffer size is 40
 *
 * @return 0 if dst was not enough or number of printed characters
 */
size_t tm_human_print(ts_time_t t, const char* dst, size_t size) {
	struct tm_component components[] = {
		{ TS_TIME_SEC(t), "s" },
		{ TS_TIME_MS(t), "ms" },
		{ TS_TIME_US(t), "us" },
		{ TS_TIME_NS(t), "ns" }
	};

	ts_time_t tm;

	int i = 0;
	size_t length = 0;

	/* Maximum number of characters needed to keep seconds */
	if(size < 14)
		return 0;

	/* 7 chars are enough to store thousands + suffix + space + null-term */
	for(; i < 4; ++i) {
		if(length > (size - 8))
			return 0;

		tm = components[i].tm;

		if(tm > 0) {
			length += snprintf(dst + length, size - length, "%d%s ",
							(int) tm, components[i].suffix);
		}
	}

	return length;
}

/**
 * Prints date-time in nice format (%H:%M:%S %d.%m.%Y)
 */
size_t tm_datetime_print(ts_time_t t, const char* dst, size_t size) {
	time_t unix_start_time;

	unix_start_time = TS_TIME_TO_UNIX(t);
	return strftime(dst, size, "%H:%M:%S %d.%m.%Y", localtime(&unix_start_time));
}

