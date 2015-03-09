
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
#include <tsload/pathutil.h>

#include <hostinfo/fsinfo.h>
#include <hostinfo/diskinfo.h>

#include <hitrace.h>

#include <stdio.h>
#include <errno.h>

#include <sys/statvfs.h>

#define PROC_MOUNTS				"/proc/self/mounts"

int hi_linux_statvfs(hi_fsinfo_t* fsi) {
	struct statvfs vfsmnt;
	
	if(statvfs(fsi->fs_mountpoint, &vfsmnt) == -1) {
		hi_fs_dprintf("hi_linux_statvfs: error statvfs(%s) errno %d\n", 
					  fsi->fs_mountpoint, errno);
		return HI_PROBE_ERROR;
	}
	
	fsi->fs_block_size = vfsmnt.f_bsize;
	fsi->fs_frag_size = vfsmnt.f_frsize;
	
	fsi->fs_ino_count = vfsmnt.f_files;
	fsi->fs_ino_free = vfsmnt.f_ffree;
	
	/* Linux doesn't support fragments, so use f_bsize here */
	fsi->fs_space = vfsmnt.f_blocks * vfsmnt.f_bsize;
	fsi->fs_space_free = vfsmnt.f_bfree * vfsmnt.f_bsize;
	
	fsi->fs_namemax = vfsmnt.f_namemax;
	
	return HI_PROBE_OK;
}

void hi_linux_mounts_read(FILE* f, char* buffer, size_t buflen, int delim) {
	int c;
	char* p = buffer;
	char* end = buffer + buflen - 1;
	
	do {
		c = fgetc(f);
		if(c == delim || c == EOF)
			break;
		if(buffer && p < end)
			*p++ = c;		
	} while(!feof(f));
	
	if(buffer)
		*p++ = '\0';
}


void hi_linux_mounts_demangle(char* buffer) {
	char* p = buffer;
	char* q = buffer;
	
	while(*p) {
		if(p != q)
			*q = *p;
		if(*p == '\\') {
			/* Decode mangled symbol */
			*q = ((p[1] - '0') & 3) << 6 | 
			     ((p[2] - '0') & 7) << 3 | 
			     ((p[3] - '0') & 7);
			p += 3;
		}
		
		++p; ++q;
	}
	
	if(p != q)
		*q = *p;
}

PLATAPI int hi_fsinfo_probe_impl(list_head_t* disks) {
	FILE* mounts = fopen(PROC_MOUNTS, "r");
	hi_fsinfo_t* fsi;
	
	char devpath[PATHMAXLEN];
	char fstype[FSTYPELEN];
	char mntpt[PATHMAXLEN];
	char rorw[8];
	
	while(!feof(mounts)) {
		hi_linux_mounts_read(mounts, devpath, PATHMAXLEN, ' ');
		hi_linux_mounts_read(mounts, mntpt, PATHMAXLEN, ' ');
		hi_linux_mounts_read(mounts, fstype, FSTYPELEN, ' ');
		hi_linux_mounts_read(mounts, rorw, 8, ',');
		
		if(feof(mounts))
			break;
		
		hi_linux_mounts_demangle(devpath);
		hi_linux_mounts_demangle(mntpt);
		
		hi_fs_dprintf("hi_fsinfo_probe_impl: Found %s %s mntpt=%s, devpath=%s\n", 
					  rorw, fstype, mntpt, devpath);
		
		hi_linux_mounts_read(mounts, NULL, 0, '\n');
		
		/* Ignore rootfs because it is later remounted with correct device */
		if(strcmp(fstype, "rootfs") == 0)
			continue;
		
		/* There are a lot of service file systems in Linux 
		 * that are mounted in /dev/, /sys/ or /proc/ which we
		 * do not want to show (because they are irrelevant in 
		 * context of TSLoad, and just pollute list).
		 * Ignore them. */
		if(strncmp(mntpt, "/dev/", 5) == 0 ||
				strncmp(mntpt, "/sys/", 5) == 0 ||
				strncmp(mntpt, "/proc/", 6) == 0) 
			continue;
		
		fsi = hi_fsinfo_create(mntpt, fstype, devpath);
		
		fsi->fs_readonly = strcmp(rorw, "rw") != 0;
		hi_linux_statvfs(fsi);
		hi_fsinfo_bind_dev(fsi, disks);
		
		hi_fsinfo_add(fsi);
	}
	
	fclose(mounts);	
	return HI_PROBE_OK;
}