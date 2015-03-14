
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
#include <tsload/readlink.h>
#include <tsload/posixdecl.h>

#include <hostinfo/fsinfo.h>
#include <hostinfo/diskinfo.h>

#include <hitrace.h>

mp_cache_t hi_fsinfo_cache;

hi_fsinfo_t* hi_fsinfo_create(const char* mntpt, const char* fstype, 
							  const char* devpath) {
	hi_fsinfo_t* fsi = mp_cache_alloc(&hi_fsinfo_cache);
	
	strncpy(fsi->fs_type, fstype, FSTYPELEN);
	if(devpath)
		aas_copy(aas_init(&fsi->fs_devpath), devpath);
	
	fsi->fs_device = NULL;
	aas_init(&fsi->fs_host);
	
	fsi->fs_block_size = 0;
	fsi->fs_frag_size = 0;
	
	fsi->fs_ino_count = 0;
	fsi->fs_ino_free = 0;
	fsi->fs_space = 0;
	fsi->fs_space_free = 0;
	
	fsi->fs_namemax = 0;
	fsi->fs_readonly = B_TRUE;
	
	hi_obj_header_init(HI_SUBSYS_FS, &fsi->fs_hdr, mntpt);
	
	return fsi;
}

void hi_fsinfo_dtor(hi_object_header_t* object) {
	hi_fsinfo_t* fsi = HI_FSINFO_FROM_OBJ(object);
	
	/* FIXME: release fs_device here  */
	
	aas_free(&fsi->fs_devpath);
	aas_free(&fsi->fs_host);
	
	mp_cache_free(&hi_fsinfo_cache, fsi);
}

int hi_fsinfo_probe(void) {
	list_head_t* disks = hi_dsk_list(B_FALSE);
	if(!disks)
		return HI_PROBE_ERROR;
	
	return hi_fsinfo_probe_impl(disks);
}

tsobj_node_t* tsobj_hi_fsinfo_format(struct hi_object_header* object) {
	hi_fsinfo_t* fsi = HI_FSINFO_FROM_OBJ(object);
	tsobj_node_t* fsinfo = tsobj_new_node("tsload.hi.FileSystem");
	
	tsobj_add_string(fsinfo, TSOBJ_STR("devpath"), tsobj_str_create(fsi->fs_devpath));
	
	if(fsi->fs_device)
		tsobj_add_string(fsinfo, TSOBJ_STR("device"), 
						 tsobj_str_create(fsi->fs_device->d_disk_name));

	tsobj_add_string(fsinfo, TSOBJ_STR("fstype"), tsobj_str_create(fsi->fs_type));
	if(fsi->fs_host)
		tsobj_add_string(fsinfo, TSOBJ_STR("host"), tsobj_str_create(fsi->fs_host));

	tsobj_add_integer(fsinfo, TSOBJ_STR("block_size"), fsi->fs_block_size);
	tsobj_add_integer(fsinfo, TSOBJ_STR("frag_size"), fsi->fs_frag_size);
	
	tsobj_add_integer(fsinfo, TSOBJ_STR("ino_count"), fsi->fs_ino_count);
	tsobj_add_integer(fsinfo, TSOBJ_STR("ino_free"), fsi->fs_ino_free);
	tsobj_add_integer(fsinfo, TSOBJ_STR("space"), fsi->fs_space);
	tsobj_add_integer(fsinfo, TSOBJ_STR("space_free"), fsi->fs_space_free);
	tsobj_add_integer(fsinfo, TSOBJ_STR("namemax"), fsi->fs_namemax);
	
	tsobj_add_boolean(fsinfo, TSOBJ_STR("readonly"), fsi->fs_readonly);
	
	return fsinfo;
}

/* hi_fsinfo_bind_dev()
 * -------------------- */

void hi_fsinfo_bind_dev_impl(hi_fsinfo_t* fsi, list_head_t* disks, const char* path) {
	char linkpath[PATHMAXLEN];
	struct stat linkstat;
	
	hi_dsk_info_t* di;
	hi_object_t* object;
	
	hi_fs_dprintf("hi_fsinfo_bind_dev_impl: Trying to bind FS %s at %s\n", 
				  fsi->fs_mountpoint, path);
	
	hi_for_each_object(object, disks) {
		di = HI_DSK_FROM_OBJ(object);
		
		if(di->d_path && strcmp(di->d_path, path) == 0) {
			/* FIXME: hold device here */
			fsi->fs_device = di;
			return;
		}
	}
	
	if(stat(path, &linkstat) == -1)
		return;
	if(path_abslink(linkpath, PATHMAXLEN, path) == NULL)
		return;
	
	/* Recursively call it with symlink */
	hi_fsinfo_bind_dev_impl(fsi, disks, linkpath);
}

/**
 * Find appropriate device in disks hiobject hierarchy and 
 * bind it to filesystem information object. 
 * 
 * @param fsi filesystem information object
 * @param disks list of disks received from `hi_dsk_list()`
 * 
 * @note call it from `hi_fsinfo_probe_impl()` context
 */
void hi_fsinfo_bind_dev(hi_fsinfo_t* fsi, list_head_t* disks) {
	hi_fsinfo_bind_dev_impl(fsi, disks, fsi->fs_devpath);
}

int hi_fsinfo_init(void) {
	mp_cache_init(&hi_fsinfo_cache, hi_fsinfo_t);
	
	return 0;
}

void hi_fsinfo_fini(void) {
	mp_cache_destroy(&hi_fsinfo_cache);
}
