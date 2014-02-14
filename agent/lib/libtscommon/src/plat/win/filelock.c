/*
 * flock.c
 *
 *  Created on: Feb 11, 2014
 *      Author: myaut
 */

#include <defs.h>
#include <filelock.h>

#include <errno.h>
#include <string.h>

#include <windows.h>

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
