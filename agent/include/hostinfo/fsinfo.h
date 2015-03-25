
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

/**
 * @module FSInfo
 * 
 * Gets information on mounted filesystems. 
 * 
 * Note that unlike other HIObject subsystems, filesystems are not forming a hierarchy:
 * if mountpoint resides on other filesystem, it wont be set as child to it. 
 * 
 * Since usually filesystems reside on disk devices, FSInfo object contains reference
 * to corresponding DiskInfo object, and FSInfo probing causes DiskInfo probing as well.
 */

#define FSTYPELEN		32

/**
 * File system information descriptor
 * 
 * @member fs_hdr			HIObject header
 * @member fs_mountpoint	alias to `fs_hdr.name` - absolute path where filesystem is mounted
 * @member fs_type			type of filesystem
 * @member fs_devpath		path to device that holds filesystem. Not necessary a name or `d_path` \
 * 							in DiskInfo hierarchy
 * @member fs_device		corresponding DiskInfo device
 * @member fs_host			(not supported) hostname for network filesystem
 * @member fs_block_size	maximum size of filesystem block
 * @member fs_frag_size		fragments size or minimum size of block
 * @member fs_ino_count		(optional) total number of file entries that created or can be created
 * @member fs_ino_free		(optional) number of file entries that can be created
 * @member fs_space			filesystem total space in bytes
 * @member fs_space_free	filesystem free space in bytes
 * @member fs_readonly		is file system mounted in readonly mode?
 * @member fs_namemax		maximum length of filesystem name
 */
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

/**
 * Conversion macros
 */
#define HI_FSINFO_FROM_OBJ(object)		((hi_fsinfo_t*) (object))
#define HI_FSINFO_TO_OBJ(fsi) 			(&fsi->fs_hdr)

hi_fsinfo_t* hi_fsinfo_create(const char* mntpt, const char* fstype, 
							 const char* devpath);
void hi_fsinfo_dtor(hi_object_header_t* object);

LIBEXPORT hi_fsinfo_t* hi_fsinfo_find_bypath(const char* path);
LIBEXPORT hi_fsinfo_t* hi_fsinfo_find_bydev(hi_dsk_info_t* di);

void hi_fsinfo_bind_dev(hi_fsinfo_t* fsi, list_head_t* disks);

/**
 * Add filesystem information to global list */
STATIC_INLINE void hi_fsinfo_add(hi_fsinfo_t* fsi) {
	hi_obj_add(HI_SUBSYS_FS, (hi_object_header_t*) &fsi->fs_hdr);
}

/**
 * Find filesystem by it's mountpoint
 * 
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
 * @note May call `hi_dsk_list()` with reprobe = B_FALSE
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