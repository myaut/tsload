
/*
    This file is part of TSLoad.
    Copyright 2013, Sergey Klyaus, ITMO University

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
#include <log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_EXECINFO_H

#include <execinfo.h>

PLATAPI int plat_get_callers(char* callers, size_t size) {
	int nptrs, j;
	void *buffer[LOG_MAX_BACKTRACE + 2];
	char **strings;

	char* fname;
	int ret;

	nptrs = backtrace(buffer, LOG_MAX_BACKTRACE);
	strings = backtrace_symbols(buffer, nptrs);

	for(j = 2; j < nptrs; j++) {
		fname = strchr(strings[j], '(');
		fname = (fname == NULL) ? strings[j] : fname;

		ret = snprintf(callers, size, "\n\t\t%s", fname);

		if(ret < 0 || ret >= size)
			break;

		size -= ret;
		callers += ret;
	}

	free(strings);

	return 0;
}

#else

PLATAPI int plat_get_callers(char* callers, size_t size) {
	return 0;
}

#endif

