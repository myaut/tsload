
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



#include <tsload/defs.h>

#include <tsload/time.h>

#include <windows.h>


PLATAPI ts_time_t tm_get_time() {
	FILETIME time;
	ts_time_t now;

	/*See http://support.microsoft.com/kb/167296 */

	GetSystemTimeAsFileTime(&time);

	now = ((LONGLONG) time.dwHighDateTime) << 32 | time.dwLowDateTime;
	now = (now - 116444736000000000) * 100;

	return now;
}

PLATAPI ts_time_t tm_get_clock_res() {
	LARGE_INTEGER freq;
	ts_time_t t;

	QueryPerformanceFrequency(&freq);

	if(freq.QuadPart == 0) {
		/* Will use tm_get_time which is 100-ns precise */
		return 0;
	}

	return (ts_time_t) ((double) T_SEC / ((double) freq.QuadPart));
}


PLATAPI ts_time_t tm_get_clock() {
	ts_time_t res;
	ts_time_t t;
	LARGE_INTEGER clock;

	res = tm_get_clock_res();

	if(res == 0) {
		/* Fall back to unprecise time source */
		return tm_get_time();
	}

	QueryPerformanceCounter(&clock);

	return (ts_time_t) ((double) clock.QuadPart * ((double) res));
}

PLATAPI void tm_sleep_milli(ts_time_t t) {
	Sleep(t / T_MS);
}

/*NOTE: NtDelayExecution is undocumented*/
extern NTSTATUS (* NtDelayExecution)(BOOLEAN Alertable, PLARGE_INTEGER DelayInterval);

PLATAPI void tm_sleep_nano(ts_time_t t) {
	LARGE_INTEGER due_time;

	/* Minus means that value is relative to current,
	 * Also due_time is measured in 100-ns intervals*/
	due_time.QuadPart = -((LONGLONG) t / 100);

	NtDelayExecution(FALSE, &due_time);
}

