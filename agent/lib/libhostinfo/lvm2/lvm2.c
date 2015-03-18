
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

#include <hostinfo/diskinfo.h>

#include <hitrace.h>

#include <unistd.h>

#include <lvm2app.h>

/**
 * ### LVM2
 * 
 * Uses liblvm2app to get information about Logical Volume Manager 2 on Linux
 * Volume Groups (VGs) are considered as pools, while Logical Volumes (LVs) are 
 * added as volumes.
 * 
 * Hides its objects in namespace `lvm2` so naming schema is `lvm2:VGNAME[:LVNAME]`.
 * 
 * __NOTE__: LVM2 resides on top of device mapper which nodes are collected by
 * generic DiskInfo code. So, `dm-X` volumes are bound to real LVM2 volumes
 * adding extra layer of indirection:
 * 
 * __NOTE__: Access to `/dev/mapper/control` require admin privileges, so it may
 * fail entire DiskInfo probing. Set `hi_linux_lvm2` tunable to `false` to disable
 * LVM2 probing.
 */

#define DEV_MAPPER_CONTROL		"/dev/mapper/control"

lvm_t libh;

int hi_lin_proc_lvm_pv(pv_t pv, hi_dsk_info_t* vgdi, const char* vgname) {
	const char* pvname = lvm_pv_get_name(pv);
	const char* devname;
	
	hi_dsk_info_t* pvdi;
	
	path_split_iter_t iter;
	
	hi_dsk_dprintf("hi_lin_proc_lvm_pv: Found PV '%s' in VG '%s'\n", 
				   pvname, vgname);
	
	devname = path_basename(&iter, pvname);
	pvdi = hi_dsk_find(devname);
	
	if(pvdi == NULL) {
		hi_dsk_dprintf("hi_lin_proc_lvm_pv: Found PV '%s' - cannot find device '%s'\n", 
					   pvname, devname);
		return HI_PROBE_ERROR;
	}
	
	hi_dsk_attach(pvdi, vgdi);
	
	return HI_PROBE_OK;
}

int hi_lin_proc_lvm_lv(lv_t lv, hi_dsk_info_t* vgdi, const char* vgname) {
	const char* lvname = lvm_lv_get_name(lv);
	hi_dsk_info_t* lvdi = hi_dsk_create();
	
	ssize_t dmlink_sz;	
	path_split_iter_t iter;
	char dmlink[PATHMAXLEN];
	const char* dmname;
	hi_dsk_info_t* dmdi;
	
	hi_dsk_dprintf("hi_lin_proc_lvm_lv: Found LV '%s' in VG '%s'\n", 
				   lvname, vgname);
	
	aas_printf(&lvdi->d_hdr.name, "lvm2:%s:%s", vgname, lvname);
	aas_copy(&lvdi->d_id, lvm_lv_get_uuid(lv));	
	aas_printf(&lvdi->d_path, "/dev/%s/%s", vgname, lvname);
	
	aas_set(&lvdi->d_bus_type, "LVM2");
	
	lvdi->d_size = lvm_lv_get_size(lv);
	lvdi->d_type = HI_DSKT_VOLUME;
	
	hi_dsk_add(lvdi);
	hi_dsk_attach(lvdi, vgdi);
	
	/* We already discovered dm-X devices while walking /sys/block.
	   Now we want to get rid of it, so we re-link it as "lower-level volume" 
	   We cannot destroy it, because: it may be used in mnttab (weird!) */
	dmlink_sz = readlink(lvdi->d_path, dmlink, PATHMAXLEN);
	if(dmlink_sz == -1) {
		hi_dsk_dprintf("hi_lin_proc_lvm_lv: Failed to readlink '%s'\n", lvdi->d_path);
		return HI_PROBE_ERROR;
	}
	
	dmlink[dmlink_sz] = '\0';
	
	dmname = path_basename(&iter, dmlink);	
	dmdi = hi_dsk_find(dmname);
	if(dmdi != NULL) {
		hi_dsk_attach(dmdi, lvdi);
		dmdi->d_type = HI_DSKT_VOLUME;
	}
	
	return HI_PROBE_OK;
}

int hi_lin_proc_lvm_vg(const char* vgname, vg_t vg) {
	hi_dsk_info_t* vgdi;
	struct dm_list* lvs;
	struct dm_list* pvs;
	
	pv_list_t* pvl;
	lv_list_t* lvl;
	
	vgdi = hi_dsk_create();
	
	/* VGs and other pools may have similiar naming, so put vg
	   into a namespace lvm2. Also, because it is pool, it is not 
	   openable, but is usually has /dev/vgname/ directory where 
	   lvs reside - use it as "d_path" */
	aas_printf(&vgdi->d_hdr.name, "lvm2:%s", vgname);
	aas_copy(&vgdi->d_id, lvm_vg_get_uuid(vg));
	aas_printf(&vgdi->d_path, "/dev/%s/", vgname);
	
	vgdi->d_size = lvm_vg_get_size(vg);
	vgdi->d_type = HI_DSKT_POOL;
	
	aas_set(&vgdi->d_bus_type, "LVM2");
	
	hi_dsk_add(vgdi);
	
	pvs = lvm_vg_list_pvs(vg);
	if(pvs != NULL) {
		dm_list_iterate_items(pvl, pvs) {
			hi_lin_proc_lvm_pv(pvl->pv, vgdi, vgname);
		}
	}
	
	lvs = lvm_vg_list_lvs(vg);
	if(lvs != NULL) {
		dm_list_iterate_items(lvl, lvs) {
			hi_lin_proc_lvm_lv(lvl->lv, vgdi, vgname);
		}
	}
	
	return HI_PROBE_OK;
}

int hi_lin_proc_lvm_vgs(void) {
	vg_t vg;
	struct dm_list *vgnames;
	struct lvm_str_list* strl;
	const char* vgname;
	
	vgnames = lvm_list_vg_names(libh);
	dm_list_iterate_items(strl, vgnames) {
		vgname = strl->str;
		vg = lvm_vg_open(libh, vgname, "r", 0);	
		
		if(vg == NULL) {
			hi_dsk_dprintf("hi_lin_proc_lvm_vgs: Error while opening VG '%s'. Error: %s\n", 
						   vgname, lvm_errmsg(libh));
			continue;
		}
		
		hi_dsk_dprintf("hi_lin_proc_lvm_vgs: Found VG '%s'\n", vgname);
		
		hi_lin_proc_lvm_vg(vgname, vg);		
		lvm_vg_close(vg);
	}
	
	return HI_PROBE_OK;
}

int hi_lin_probe_lvm(void) {	
	hi_dsk_dprintf("hi_lin_probe_lvm: Running with LVM2 version %s\n", lvm_library_get_version());
	
	if(access(DEV_MAPPER_CONTROL, R_OK | W_OK) == -1) {
		hi_dsk_dprintf("hi_lin_probe_lvm: %s: no permissions or not exists\n", DEV_MAPPER_CONTROL);
		return HI_PROBE_ERROR;
	}
	
	libh = lvm_init(NULL);
	if(libh == NULL) {
		hi_dsk_dprintf("hi_lin_probe_lvm: Failed to open LVM2 handle\n");
		return HI_PROBE_ERROR;
	}
	
	hi_lin_proc_lvm_vgs();
	
	lvm_quit(libh);
	return HI_PROBE_OK;
}