
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

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>


/**
 * Thanks for the implementation
 * www.perkin.org.uk/posts/solaris-portability-flock.html
 */

PLATAPI int plat_flock(int fd, int operation) {
		int rc = 0;

#if defined(__GNUC__) && (defined(__sparcv9) || defined(__x86_64__))
		struct flock64 fl = {0};
#elif defined(__GNUC__) && (defined(__sparcv8) || defined(__i386__))
		struct flock fl = {0};
#else
#error "Unsupported architecture for Solaris build"
#endif


		switch (operation & (LOCK_EX|LOCK_SH|LOCK_UN)) {
		case LOCK_EX:
			fl.l_type = F_WRLCK;
			break;

		case LOCK_SH:
			fl.l_type = F_RDLCK;
			break;

		case LOCK_UN:
			fl.l_type = F_UNLCK;
			break;

		default:
			errno = EINVAL;
			return -1;
		}

		fl.l_whence = SEEK_SET;
#if defined(__GNUC__) && (defined(__sparcv9) || defined(__x86_64__))
		rc = fcntl(fd, operation & LOCK_NB ? F_SETLK : F_SETLKW, &fl);
#elif defined(__GNUC__) && (defined(__sparcv8) || defined(__i386__))
		rc = fcntl(fd, operation & LOCK_NB ? F_SETLK64 : F_SETLKW64, &fl);
#else
#error "Unsupported architecture for Solaris build"
#endif

		if (rc && (errno == EAGAIN))
			errno = EWOULDBLOCK;

		return rc;
	}


