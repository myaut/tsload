
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

