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

#ifndef HI_PLAT_LINUX_MNTTAB_H_
#define HI_PLAT_LINUX_MNTTAB_H_

#include <tsload/defs.h>

#include <string.h>

#include <mntent.h>

#define ETC_MNTTAB			MOUNTED

#define mnt_special		mnt_fsname
#define mnt_mountp		mnt_dir
#define mnt_fstype		mnt_type
#define mnt_mntopts		mnt_opts

typedef struct mntent		plat_mntent_t;

STATIC_INLINE plat_mntent_t* plat_getmntent(FILE* fp) {
	return getmntent(fp);
}

STATIC_INLINE boolean_t	plat_filter_fs(plat_mntent_t* mnt) {
	/* Ignore rootfs because it is later remounted with correct device */
	if(strcmp(mnt->mnt_fstype, "rootfs") == 0)
		return B_TRUE;
	
	/* There are a lot of service file systems in Linux 
	 * that are mounted in /dev/, /sys/ or /proc/ which we
	 * do not want to show (because they are irrelevant in 
	 * context of TSLoad, and just pollute list).
	 * Ignore them. */
	if(strncmp(mnt->mnt_mountp, "/dev/", 5) == 0 ||
			strncmp(mnt->mnt_mountp, "/sys/", 5) == 0 ||
			strncmp(mnt->mnt_mountp, "/proc/", 6) == 0) 
		return B_TRUE;
	
	return B_FALSE;
}

STATIC_INLINE boolean_t plat_fs_is_readonly(const char* mntopts) {
	return strncmp(mntopts, "rw", 2) != 0;
}

#endif