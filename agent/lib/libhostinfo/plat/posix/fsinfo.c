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

#include <tsload/autostring.h>
#include <tsload/mempool.h>

#include <hostinfo/fsinfo.h>
#include <hostinfo/diskinfo.h>
#include <hostinfo/plat/mnttab.h>

#include <hitrace.h>

#include <stdio.h>
#include <errno.h>

#include <sys/statvfs.h>

/**
 * ### POSIX
 * 
 * Uses `statvfs()` syscall to gather statistics on file systems and 
 * `getmntent()` function to iterate over mounted filesystems in file
 * `/etc/mtab` (Linux) or `/etc/mnttab` (Solaris).
 */

int hi_fsinfo_statvfs(hi_fsinfo_t* fsi) {
	struct statvfs vfsmnt;
	
	if(statvfs(fsi->fs_mountpoint, &vfsmnt) == -1) {
		hi_fs_dprintf("hi_fsinfo_statvfs: error statvfs(%s) errno %d\n", 
					  fsi->fs_mountpoint, errno);
		return HI_PROBE_ERROR;
	}
	
	fsi->fs_block_size = vfsmnt.f_bsize;
	fsi->fs_frag_size = vfsmnt.f_frsize;
	
	fsi->fs_ino_count = vfsmnt.f_files;
	fsi->fs_ino_free = vfsmnt.f_ffree;
	
	fsi->fs_space = vfsmnt.f_blocks * vfsmnt.f_frsize;
	fsi->fs_space_free = vfsmnt.f_bfree * vfsmnt.f_frsize;
	
	fsi->fs_namemax = vfsmnt.f_namemax;
	
	return HI_PROBE_OK;
}

PLATAPI int hi_fsinfo_probe_impl(list_head_t* disks) {
	plat_mntent_t* mnt;
	hi_fsinfo_t* fsi;
	
	FILE* mnttab = fopen(ETC_MNTTAB, "r");
	
	if(mnttab == NULL) {
		hi_fs_dprintf("hi_fsinfo_probe: Failed to fopen('%s')\n", MNTTAB);
		return HI_PROBE_ERROR;
	}
	
	while((mnt = plat_getmntent(mnttab)) != NULL) {
		/* ZFS should be handled in libhostinfo-zfs */
		if(strcmp(mnt->mnt_fstype, "zfs") == 0)
			continue;

		if(plat_filter_fs(mnt))
			continue;
		
		hi_fs_dprintf("hi_fsinfo_probe: Found filesystem %s %s %s %s\n", mnt->mnt_mountp, 
					  mnt->mnt_fstype, mnt->mnt_special, mnt->mnt_mntopts);
		
		fsi = hi_fsinfo_create(mnt->mnt_mountp, mnt->mnt_fstype, mnt->mnt_special);
		
		fsi->fs_readonly = plat_fs_is_readonly(mnt->mnt_mntopts);
		
		hi_fsinfo_statvfs(fsi);
		hi_fsinfo_bind_dev(fsi, disks);
		
		hi_fsinfo_add(fsi);
	}
	
	fclose(mnttab);
	return HI_PROBE_OK;
}