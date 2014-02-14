/*
 * filelock.h
 *
 *  Created on: Feb 12, 2014
 *      Author: myaut
 */

#ifndef FILELOCK_H_
#define FILELOCK_H_

#include <defs.h>

#ifdef PLAT_LINUX
#include <sys/file.h>
#endif

#ifndef LOCK_SH
#define	LOCK_SH	1	/* Shared lock.  */
#endif

#ifndef LOCK_EX
#define	LOCK_EX	2 	/* Exclusive lock.  */
#endif

#ifndef LOCK_UN
#define	LOCK_UN	8	/* Unlock.  */
#endif

#ifndef LOCK_NB
#define	LOCK_NB	4	/* Non-blocking lock.  */
#endif

/**
 * flock - Locking files
 */
LIBEXPORT PLATAPI int plat_flock(int fd, int operation);

#endif /* FILELOCK_H_ */
