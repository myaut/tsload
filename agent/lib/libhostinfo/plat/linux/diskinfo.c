
/*
    This file is part of TSLoad.
    Copyright 2013, Sergey Klyaus, ITMO University

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

#include <tsload/dirent.h>
#include <tsload/pathutil.h>
#include <tsload/autostring.h>
#include <tsload/posixdecl.h>
#include <tsload/modules.h>

#include <hostinfo/diskinfo.h>
#include <hostinfo/plat/sysfs.h>

#include <hitrace.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>


/**
 * diskinfo (Linux)
 *
 * Walks /sys/block and searches for dm- or sda/hda devices
 *
 * TODO: fallback to /proc/partitions
 * TODO: d_port identification
 * */

/**
 * Disables LVM2 devices probing. 
 * Useful if HostInfo consumer is running from non-root user.
 * 
 * Even if disabled, LVs may be tracked as dm-X devices.
 * 
 * NOTE: disabling it makes LVM2 structure untrackable, thus causing unwanted
 * consequences, i.e. allowing `simpleio` module writing on disk owned by LVM.
 */
boolean_t hi_linux_lvm2 = B_TRUE;

hi_obj_helper_t hi_lvm2_helper;

#define LIBHI_LVM_LIB		"libhostinfo-lvm2.so"
#define LIBHI_LVM_OP		"hi_lin_probe_lvm"

#define DEV_ROOT_PATH		"/dev"
#define SYS_BLOCK_PATH		"/sys/block"
#define SYS_SCSI_HOST_PATH	"/sys/class/scsi_host"

/* DISK_MAX_PARTS + 1  */
#define INVALID_PARTITION_ID	257

/**
 * @return: -1 if device not exists or access mask
 * */
int hi_linux_make_dev_path(const char* name, char** aas_dev_path) {
	char* p;
	char* dev_path;

	int mode = R_OK;

	if(path_join_aas(aas_dev_path, DEV_ROOT_PATH, name, NULL) == NULL)
		return -1;

	dev_path = *aas_dev_path;

	/* in /sys/block, '/'s are replaced with '!' or '.' */
	p = dev_path;
	while(*p) {
		if(*p == '!' || *p == '.')
			*p = '/';
		p++;
	}

	if(access(dev_path, R_OK) != 0) {
		if(errno != EACCES) {
			return -1;
		}
		return 0;
	}

	if(access(dev_path, W_OK) == 0) {
		mode |= W_OK;
	}

	return mode;
}

hi_dsk_info_t* hi_linux_disk_create(const char* name, const char* dev_path, const char* sys_path, unsigned sector_size) {
	hi_dsk_info_t* di = hi_dsk_create();

	aas_copy(&di->d_hdr.name, name);
	aas_copy(&di->d_path, dev_path);
	
	di->d_size = hi_linux_sysfs_readuint(SYS_BLOCK_PATH, sys_path, "size", 0) * sector_size;

	return di;
}

void hi_linux_disk_proc_partition(const char* disk_name, const char* part_name, hi_dsk_info_t* parent, unsigned sector_size) {
	hi_dsk_info_t* di;
	char* dev_path = NULL;
	char* sys_path = NULL;

	int mode;

	char* sys_part_name = NULL;

	/* Partitions are located in /sys/block/sda/sdaX
	 * while their dev_path is /dev/sdaX, so
	 * they have logic that differs from disks */
	aas_printf(&sys_part_name, "%s/%s", disk_name, part_name);

	mode = hi_linux_make_dev_path(part_name, &dev_path);

	hi_dsk_dprintf("hi_linux_disk_proc_partition: Probing %s (%s)\n", dev_path, part_name);

	if(mode == -1) {
		goto end;
	}

	di = hi_linux_disk_create(part_name, dev_path, sys_part_name, sector_size);
	di->d_type = HI_DSKT_PARTITION;
	di->d_mode = mode;

	hi_dsk_add(di);
	hi_dsk_attach(di, parent);

end:
	aas_free(&dev_path);
	aas_free(&sys_path);
	aas_free(&sys_part_name);
}

void hi_linux_disk_proc_partitions(const char* disk_name, hi_dsk_info_t* parent, unsigned sector_size) {
	AUTOSTRING char* disk_path;
	uint64_t part_id;
	
	plat_dir_t* dir;
	plat_dir_entry_t* de;
	
	/* Seek for partitions that are located in /sys/block/sda/sdaX
	 * as a directories containing "partiton" file. */
	path_join_aas(aas_init(&disk_path), SYS_BLOCK_PATH, disk_name, NULL);
	
	dir = plat_opendir(disk_path);	
	
	while((de = plat_readdir(dir)) != NULL) {
		if(plat_dirent_type(de) != DET_DIR)
			continue;
		
		part_id = hi_linux_sysfs_readuint(disk_path, de->d_name, 
										  "partition", INVALID_PARTITION_ID);
		if(part_id == INVALID_PARTITION_ID)
			continue;
		
		hi_linux_disk_proc_partition(disk_name, de->d_name, parent, sector_size);
	}	
	
	plat_closedir(dir);
}


/* Use scsi_host --> proc_name to determine name of real 
 * SCSI bus behind this disk instance.
 * 
 * NOTE: Linux doesn't distinguish FC/SATA/SAS, so it would be driver name*/
void hi_linux_disk_get_bus(hi_dsk_info_t* di) {
	int hostid;
	char* endptr;
	char hostname[16];
	
	if(!di->d_port)
		return;
	
	hostid = (int) strtol(di->d_port, &endptr, 10);	
	if(*endptr != ':')
		return;
	
	hi_dsk_dprintf("hi_linux_disk_get_bus: Got host %d for device %s\n",
				   hostid, di->d_hdr.name);
	
	snprintf(hostname, 16, "host%d", hostid);
	
	if(hi_linux_sysfs_readstr_aas(SYS_SCSI_HOST_PATH, hostname, "proc_name",
								  &di->d_bus_type) == HI_LINUX_SYSFS_OK) {
		hi_linux_sysfs_fixstr(di->d_bus_type);
	}
}

void hi_linux_disk_proc_disk(const char* name, void* arg) {
	hi_dsk_info_t* di;
	char* dev_path;

	int mode;
	
	unsigned sector_size = (unsigned) 
			hi_linux_sysfs_readuint(SYS_BLOCK_PATH, name, 
									"queue/hw_sector_size", 512);

	mode = hi_linux_make_dev_path(name, &dev_path);

	hi_dsk_dprintf("hi_linux_disk_proc_disk: Probing %s (%s), mode: %x\n", dev_path, name, mode);

	if(mode == -1) {
		aas_free(&dev_path);
		return;
	}

	di = hi_linux_disk_create(name, dev_path, name, sector_size);

	if(strncmp(name, "dm", 2) == 0) {
		/* Device-mapper node. All dm-nodes
		 * are considered as volumes until slaves are checked */
		di->d_type = HI_DSKT_VOLUME;
		aas_set(&di->d_bus_type, "DM");

		if(hi_linux_sysfs_readstr_aas(SYS_BLOCK_PATH, name, 
									  "dm/uuid", &di->d_id) == HI_LINUX_SYSFS_OK) {			
			hi_linux_sysfs_fixstr(di->d_id);
		}
	}
	else if(strncmp(name, "ram", 3) == 0) {
		di->d_type = HI_DSKT_VOLUME;
		aas_set(&di->d_bus_type, "RAM");
	}
	else if(strncmp(name, "loop", 4) == 0) {
		di->d_type = HI_DSKT_VOLUME;
		aas_set(&di->d_bus_type, "LOOP");
	}
	else if(strncmp(name, "md", 2) == 0) {
		di->d_type = HI_DSKT_VOLUME;
		aas_set(&di->d_bus_type, "MD");
		
		hi_linux_disk_proc_partitions(name, di, sector_size);
	}
	else {
		/* sdX device - handle model, bus type, port */
		if((strncmp(name, "sd", 2) == 0)) {
			hi_linux_sysfs_readstr_aas(SYS_BLOCK_PATH, name, "device/model", &di->d_model);
			hi_linux_sysfs_fixstr(di->d_model);
			
			hi_linux_sysfs_readlink_aas(SYS_BLOCK_PATH, name, "device", &di->d_port, B_TRUE);
			
			hi_linux_disk_get_bus(di);
		}

		di->d_type = HI_DSKT_DISK;
		
		hi_linux_disk_proc_partitions(name, di, sector_size);
	}

	hi_dsk_add(di);

	aas_free(&dev_path);
}

void hi_linux_disk_proc_slave(const char* name, void* arg) {
	char* parent_name = (char*) arg;

	hi_dsk_info_t *parent, *di;

	parent = hi_dsk_find(parent_name);
	di = hi_dsk_find(name);

	if(parent == NULL || di == NULL)
		return;

	hi_dsk_attach(di, parent);

	/* Resolve device mapper pools */
	if(di->d_type == HI_DSKT_DISK && parent->d_type == HI_DSKT_VOLUME)
		parent->d_type = HI_DSKT_POOL;
}

void hi_linux_disk_proc_slaves(const char* name, void* arg) {
	char slaves_path[128];

	path_join(slaves_path, 128, SYS_BLOCK_PATH, name, "slaves", NULL);

	hi_linux_sysfs_walk(slaves_path, hi_linux_disk_proc_slave, name);
}

int hi_lin_probe_lvm(void) {
	int ret = HI_PROBE_OK;
	
	if(!hi_linux_lvm2) {
		/* LVM2 is intentionally disabled */
		hi_dsk_dprintf("hi_lin_probe_lvm: LVM2 is disabled via tunable\n");
		return HI_PROBE_OK;
	}
	
	if(hi_lvm2_helper.loaded) {
		ret = hi_lvm2_helper.op_probe();
	}
	
	return ret;
}

PLATAPI int hi_dsk_probe(void) {
	int ret;

	ret = hi_linux_sysfs_walk(SYS_BLOCK_PATH, hi_linux_disk_proc_disk, NULL);
	if(ret != HI_LINUX_SYSFS_OK)
		return HI_PROBE_ERROR;

	ret = hi_linux_sysfs_walk(SYS_BLOCK_PATH, hi_linux_disk_proc_slaves, NULL);
	if(ret != HI_LINUX_SYSFS_OK)
		return HI_PROBE_ERROR;
	
	ret = hi_lin_probe_lvm();
	if(ret != HI_PROBE_OK)
		return ret;

	return HI_PROBE_OK;
}

PLATAPI int plat_hi_dsk_init(void) {
	int ret = hi_obj_load_helper(&hi_lvm2_helper, LIBHI_LVM_LIB, LIBHI_LVM_OP);
	
	return ret;
}

PLATAPI void plat_hi_dsk_fini(void) {
	hi_obj_unload_helper(&hi_lvm2_helper);
	
	return;
}