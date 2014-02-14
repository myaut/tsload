/*
 * filelock.c
 *
 *  Created on: Feb 12, 2014
 *      Author: myaut
 */

#include <filelock.h>

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

#if defined(__GNUC__) && defined(__sparc_v9__) || defined(__x86_64__)
		struct flock64 fl = {0};
#elif defined(__GNUC__) && defined(__i386__)
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
#if defined(__GNUC__) && defined(__sparc_v9__) || defined(__x86_64__)
		rc = fcntl(fd, operation & LOCK_NB ? F_SETLK : F_SETLKW, &fl);
#elif defined(__GNUC__) && defined(__i386__)
		rc = fcntl(fd, operation & LOCK_NB ? F_SETLK64 : F_SETLKW64, &fl);
#else
#error "Unsupported architecture for Solaris build"
#endif

		if (rc && (errno == EAGAIN))
			errno = EWOULDBLOCK;

		return rc;
	}

