/*
    This file is part of TSLoad.
    Copyright 2015, Sergey Klyaus, Tune-IT

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

#ifndef HI_PLAT_SOLARIS_MNTTAB_H_
#define HI_PLAT_SOLARIS_MNTTAB_H_

#include <tsload/defs.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mnttab.h>

#define ETC_MNTTAB			MNTTAB

typedef struct mnttab 		plat_mntent_t;

STATIC_INLINE plat_mntent_t* plat_getmntent(FILE* fp) {
	static plat_mntent_t mnt;
	
	int ret = getmntent(fp, &mnt);
	
	if(ret == 0)
		return &mnt;
	return NULL;
}

STATIC_INLINE boolean_t	plat_filter_fs(plat_mntent_t* mnt) {
	struct stat st;
	
	/* Ignore mounting of single file (dfstab, libc, lofs) */
	if(stat(mnt->mnt_mountp, &st) == 0 && S_ISREG(st.st_mode))
		return B_TRUE;
	
	return B_FALSE;
}

#endif