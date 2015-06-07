
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

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

#include <tsload/filelock.h>

#include <errno.h>
#include <string.h>

#include <windows.h>
#include <io.h>

PLATAPI int plat_flock(int fd, int operation) {
	HANDLE h = _get_osfhandle(fd);

	DWORD sz_lower;
	DWORD sz_upper;

	BOOL result;

	int flags = 0;
	OVERLAPPED ovlp;

	if(h == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	if(operation & LOCK_NB) {
		operation &= ~LOCK_NB;
		flags |= LOCKFILE_FAIL_IMMEDIATELY;
	}

	sz_lower = GetFileSize(h, &sz_upper);
	memset(&ovlp, 0, sizeof(OVERLAPPED));

	if(operation == LOCK_UN) {
		result = UnlockFile(h, 0, 0, sz_lower, sz_upper);
	}
	else {
		if(operation == LOCK_EX) {
			flags |= LOCKFILE_EXCLUSIVE_LOCK;
		}
		else if( operation != LOCK_SH) {
			errno = EINVAL;
			return -1;
		}

		result = LockFileEx(h, flags, 0, sz_lower, sz_upper, &ovlp);
	}

	if(!result)
		return -1;

	return 0;
}

