
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

#include <tsload/defs.h>

#include <tsload/posixdecl.h>

#include <hostinfo/fsinfo.h>

#include <hiprint.h>

#include <stdio.h>

int print_fs_info(int flags) {
	list_head_t* fslist;
	hi_fsinfo_t* fsi;
	hi_object_t* object;
	
	char devstr[256];
	
	fslist = hi_fsinfo_list(B_FALSE);
	if(fslist == NULL)
		return INFO_ERROR;

	print_header(flags, "FILESYSTEM INFORMATION");
	if(flags & INFO_LEGEND) {
		printf("%-32s %-8s %s\n", "DEVICE", "TYPE", "MOUNTPOINT");
	}
	
	hi_for_each_object(object, fslist) {
		fsi = HI_FSINFO_FROM_OBJ(object);
		
		if(fsi->fs_device) {
			snprintf(devstr, 256, "%s(%s)", fsi->fs_device->d_disk_name, 
					 fsi->fs_devpath);
			printf("%-32s ", devstr);
		}
		else {
			printf("%-32s ", fsi->fs_devpath);
		}
		
		printf("%-8s %s\n", fsi->fs_type, fsi->fs_mountpoint);
		
		if(!(flags & INFO_XDATA))
			continue;
		
		printf("\tbsize        %-16lu frsize %lu\n", fsi->fs_block_size, fsi->fs_frag_size);
		printf("\tinodes free: %-16lu total: %-16lu\n", fsi->fs_ino_free, fsi->fs_ino_count);
		printf("\tspace  free: %-16" PRId64 " total: %-16" PRId64 "\n", 
			   fsi->fs_space_free, fsi->fs_space);
		printf("\tnamemax %-10lu %s\n", fsi->fs_namemax, 
			   (fsi->fs_readonly)? "READONLY" : "");
	}
	
	return INFO_OK;
}