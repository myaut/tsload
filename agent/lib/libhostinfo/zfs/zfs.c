
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

#include <tsload/pathutil.h>
#include <tsload/autostring.h>
#include <tsload/posixdecl.h>

#include <hostinfo/fsinfo.h>
#include <hostinfo/diskinfo.h>

#include <hitrace.h>

#include <unistd.h>
#include <libzfs.h>
#include <libnvpair.h>
#include <sys/statvfs.h>
#include <errno.h>

void hi_zfs_proc_fs(zfs_handle_t* zfs, const char* zname, hi_dsk_info_t* zpdi) {
	hi_fsinfo_t* fsi;
	char mountpoint[PATHMAXLEN];
	int ret;
	
	uint64_t used, avail;
	
	struct statvfs vfsmnt;
	
	ret = zfs_prop_get(zfs, ZFS_PROP_MOUNTPOINT, mountpoint, PATHMAXLEN,
					   NULL, NULL, 0, B_FALSE);
	
	if(ret || strcmp(mountpoint, "none") == 0 || 
			  strcmp(mountpoint, "legacy") == 0) {
		return;
	}
	
	if(statvfs(mountpoint, &vfsmnt) == -1) {
		hi_fs_dprintf("hi_zfs_proc_fs: error statvfs(%s) errno %d\n", mountpoint, errno);
		return HI_PROBE_ERROR;
	}
	
	fsi = hi_fsinfo_create(mountpoint, "zfs", zname);
	fsi->fs_device = zpdi;
	
	fsi->fs_block_size = vfsmnt.f_bsize;
	fsi->fs_frag_size = vfsmnt.f_frsize;
	
	fsi->fs_ino_count = vfsmnt.f_files;
	fsi->fs_ino_free = vfsmnt.f_ffree;
	
	/* Don't use USED here because it accounts children (which we don't want) */
	used = zfs_prop_get_int(zfs, ZFS_PROP_REFERENCED);
	avail = zfs_prop_get_int(zfs, ZFS_PROP_AVAILABLE); 
	
	fsi->fs_space = used + avail;
	fsi->fs_space_free = avail;
	
	fsi->fs_namemax = vfsmnt.f_namemax;
	
	fsi->fs_readonly = zfs_prop_get_int(zfs, ZFS_PROP_READONLY);
	
	hi_fsinfo_add(fsi);
}

void hi_zfs_proc_zvol(zfs_handle_t* zfs, const char* zname, hi_dsk_info_t* zpdi) {
	hi_dsk_info_t* zvdi = hi_dsk_create();
	
	aas_printf(&zvdi->d_disk_name, "%s:%s", zpdi->d_disk_name, zname);
	aas_copy(&zvdi->d_bus_type, "ZPOOL:ZVOL");
	
	aas_printf(&zvdi->d_path, "/dev/zvol/dsk/%s", zname);
	
	zvdi->d_size = zfs_prop_get_int(zfs, ZFS_PROP_VOLSIZE);
	zvdi->d_type = HI_DSKT_VOLUME;
	
	hi_dsk_add(zvdi);
	hi_dsk_attach(zvdi, zpdi);
}

int hi_zfs_zfs_walker(zfs_handle_t* zfs, void* data) {
	zfs_type_t type = zfs_get_type(zfs);
	const char* zname = zfs_get_name(zfs);
	
	const char* zpname = zpool_get_name(zfs_get_pool_handle(zfs));
	AUTOSTRING char* dname;
	hi_dsk_info_t* zpdi = NULL;
	
	hi_dsk_dprintf("hi_zfs_zfs_walker: Found zfs dataset '%s' of type %d on pool '%s'\n", 
				   zname, type, zpname);
	
	/* Find appropriate zpool */
	aas_printf(aas_init(&dname), "zfs:%s", zpname);
	zpdi = hi_dsk_find(dname);
	
	if(zpdi == NULL) {
		hi_dsk_dprintf("hi_zfs_zfs_walker: Cannot find pool '%s'\n", dname);
		aas_free(&dname);
		return 0;
	}
	
	aas_free(&dname);
	
	if(type == ZFS_TYPE_VOLUME) {
		hi_zfs_proc_zvol(zfs, zname, zpdi);
	}
	else if(type == ZFS_TYPE_FILESYSTEM) {
		hi_zfs_proc_fs(zfs, zname, zpdi);
	}
	else {
		return 0;
	}
	
	zfs_iter_children(zfs, hi_zfs_zfs_walker, NULL);
	
	return 0;
}

void hi_zfs_bind_children(zpool_handle_t* zp, hi_dsk_info_t* parent, 
						  nvlist_t* vdev_tree, hi_dsk_info_t* zpdi, const char* nvkey) {
	nvlist_t **vdev;
	uint_t vdid, vd_count;
	uint64_t vd_asize;
	uint64_t vd_is_log = B_FALSE;
	char* vdname;
	
	boolean_t is_leaf_vdev;
	
	hi_dsk_info_t* vddi = NULL;
	
	libzfs_handle_t *hdl = zpool_get_handle(zp);
	
	if(nvlist_lookup_nvlist_array(vdev_tree, nvkey, &vdev, &vd_count) != 0) {
		hi_dsk_dprintf("hi_zfs_bind_children: Failed to get %s for pool '%s'\n", 
					   nvkey, parent->d_disk_name);
		return;
	}
	
	for(vdid = 0; vdid < vd_count; ++vdid) {
#ifdef HAVE_ZPOOL_VDEV_NAME_V2
		vdname = zpool_vdev_name(hdl, zp, vdev[vdid], B_TRUE, B_FALSE);
#else
		vdname = zpool_vdev_name(hdl, zp, vdev[vdid], B_TRUE);
#endif
		/* Seek for vdev. For leaf vdevs we _should_ find corresponding 
		   /dev/dsk node that we found earlier in diskinfo code. */
		is_leaf_vdev = !nvlist_exists(vdev[vdid], ZPOOL_CONFIG_CHILDREN);
		
		if(is_leaf_vdev) {
			vddi = hi_dsk_find(vdname);
		}
		else {
			vddi = hi_dsk_create();
			
			aas_printf(&vddi->d_disk_name, "%s:%s", parent->d_disk_name, vdname);
			aas_copy(&vddi->d_bus_type, "ZPOOL:VDEV");
			
			aas_copy(&vddi->d_path, "/dev/zfs");
			
			vddi->d_type = HI_DSKT_POOL;
			
			hi_zfs_bind_children(zp, vddi, vdev[vdid], NULL, ZPOOL_CONFIG_CHILDREN);
			
			hi_dsk_add(vddi);
			
			/* Account zpool size. 
			 * Assume that asize(ZPOOL) = sum(asize of top-level vdevs).
			 * 
			 * Ignore asize for log/cache devices. */
			nvlist_lookup_uint64(vdev[vdid], ZPOOL_CONFIG_IS_LOG, &vd_is_log);
			if(nvlist_lookup_uint64(vdev[vdid], ZPOOL_CONFIG_ASIZE, &vd_asize) == 0) {
				if(!vd_is_log && zpdi) {
					zpdi->d_size += vd_asize;
				}
				
				vddi->d_size = vd_asize;
			}
		}
		
		if(vddi) {
			hi_dsk_attach(vddi, parent);
		}
		
		hi_dsk_dprintf("hi_zfs_bind_children: Created/found diskinfo for vdev '%s' (%s) -> %s\n", vdname,
						is_leaf_vdev? "leaf" : "branch", vddi ? "ok" : "error");
		
		free(vdname);
	}
}

int hi_zfs_zpool_walker(zpool_handle_t* zp, void* arg) {
	const char* zpname = zpool_get_name(zp);
	
	int ret;
	
	nvlist_t* vdev_tree;
	
	hi_dsk_info_t* zpdi;
	
	hi_dsk_dprintf("hi_zfs_zpool_walker: Found zfs pool '%s'\n", zpname);
	
	/* Create diskinfo */
	zpdi = hi_dsk_create();
	
	aas_printf(&zpdi->d_disk_name, "zfs:%s", zpname);
	aas_copy(&zpdi->d_bus_type, "ZPOOL");
	
	aas_copy(&zpdi->d_path, "/dev/zfs");
	
	zpdi->d_type = HI_DSKT_POOL;
	
	hi_dsk_add(zpdi);
	
	/* Walk VDEVs */
	ret = nvlist_lookup_nvlist(zpool_get_config(zp, NULL), 
							   ZPOOL_CONFIG_VDEV_TREE, &vdev_tree);
	if(ret != 0) {
		hi_dsk_dprintf("hi_zfs_zpool_walker: Failed to get VDEV_TREE for pool '%s'\n", zpname);
		return 0;
	}
	
	hi_zfs_bind_children(zp, zpdi, vdev_tree, zpdi, ZPOOL_CONFIG_CHILDREN);
	hi_zfs_bind_children(zp, zpdi, vdev_tree, NULL, ZPOOL_CONFIG_SPARES);
	hi_zfs_bind_children(zp, zpdi, vdev_tree, NULL, ZPOOL_CONFIG_L2CACHE);
	
	return 0;
}

int hi_zfs_probe(void) {
	libzfs_handle_t* zfshdl = libzfs_init();
	
	if(zfshdl == NULL) {
		hi_dsk_dprintf("hi_zfs_probe: Failed to initialize libzfs handle!\n");
		return HI_PROBE_ERROR;
	}
	
	libzfs_print_on_error(zfshdl, B_FALSE);
	
	/* Iterate over pools */
	zpool_iter(zfshdl, hi_zfs_zpool_walker, NULL);
	zfs_iter_root(zfshdl, hi_zfs_zfs_walker, NULL);
	
	libzfs_fini(zfshdl);
	
	return HI_PROBE_OK;
}