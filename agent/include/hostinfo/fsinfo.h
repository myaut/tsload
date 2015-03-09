
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

#ifndef FSINFO_H_
#define FSINFO_H_

#include <tsload/defs.h>

#include <tsload/list.h>

#include <hostinfo/hiobject.h>
#include <hostinfo/diskinfo.h>

#define FSTYPELEN		32

typedef struct hi_fsinfo {
	hi_object_header_t fs_hdr;
#define fs_mountpoint  fs_hdr.name
	
	char fs_type[FSTYPELEN];
	AUTOSTRING char* fs_devpath;
	
	hi_dsk_info_t* fs_device;
	
	AUTOSTRING char* fs_host;
	
	/* Stats */
	unsigned long fs_block_size;
	unsigned long fs_frag_size;
	
	uint64_t fs_ino_count;
	uint64_t fs_ino_free;
	uint64_t fs_space;
	uint64_t fs_space_free;
	
	boolean_t fs_readonly;
	
	unsigned long fs_namemax;
} hi_fsinfo_t;

#define HI_FSINFO_FROM_OBJ(object)		((hi_fsinfo_t*) (object))

hi_fsinfo_t* hi_fsinfo_create(const char* mntpt, const char* fstype, 
							 const char* devpath);
void hi_fsinfo_dtor(hi_object_header_t* object);

void hi_fsinfo_bind_dev(hi_fsinfo_t* fsi, list_head_t* disks);

/**
 * Add filesystem information to global list */
STATIC_INLINE void hi_fsinfo_add(hi_fsinfo_t* fsi) {
	hi_obj_add(HI_SUBSYS_FS, (hi_object_header_t*) &fsi->fs_hdr);
}

/**
 * Find filesystem by it's mountpoint
 * @param name - name of disk
 *
 * @return filesystem information or NULL if it wasn't found
 * */
STATIC_INLINE hi_fsinfo_t* hi_fsinfo_find(const char* mntpt) {
	return (hi_fsinfo_t*) hi_obj_find(HI_SUBSYS_FS, mntpt);
}

/**
 * Probe system's filesystems (if needed) and return pointer
 * to global filesystem info list.
 *
 * @param reprobe Probe system's filesystems again
 *
 * @return pointer to head or NULL if probe failed
 * 
 * @note May call hi_dsk_list() with reprobe = B_FALSE
 */
STATIC_INLINE list_head_t* hi_fsinfo_list(boolean_t reprobe) {
	return hi_obj_list(HI_SUBSYS_FS, reprobe);
}

int hi_fsinfo_probe(void);

PLATAPI int hi_fsinfo_probe_impl(list_head_t* disks);

int hi_fsinfo_init(void);
void hi_fsinfo_fini(void);

tsobj_node_t* tsobj_hi_fsinfo_format(struct hi_object_header* object);

#endif