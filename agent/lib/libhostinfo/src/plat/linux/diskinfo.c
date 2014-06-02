
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



#include <diskinfo.h>
#include <tsdirent.h>
#include <plat/posixdecl.h>
#include <pathutil.h>
#include <plat/sysfs.h>

#include <stdlib.h>
#include <string.h>

#include <errno.h>

/**
 * diskinfo (Linux)
 *
 * Walks /sys/block and searches for dm- or sda/hda devices
 *
 * TODO: MD-RAID
 * TODO: fallback to /proc/partitions
 * TODO: Disks that are in use?
 * TODO: d_port identification
 * */

#define DEV_ROOT_PATH		"/dev"
#define SYS_BLOCK_PATH		"/sys/block"

/**
 * @return: -1 if device not exists or access mask
 * */
int hi_linux_make_dev_path(const char* name, char* dev_path) {
	char* p;
	int mode = R_OK;

	path_join(dev_path, 256, DEV_ROOT_PATH, name, NULL);

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

hi_dsk_info_t* hi_linux_disk_create(const char* name, const char* dev_path, const char* sys_path) {
	hi_dsk_info_t* di = hi_dsk_create();

	strncpy(di->d_name, name, HIDSKNAMELEN);
	strncpy(di->d_path, dev_path, HIDSKPATHLEN);
	di->d_size = hi_linux_sysfs_readuint(SYS_BLOCK_PATH, sys_path, "size", 0);

	return di;
}

void hi_linux_disk_proc_partition(const char* disk_name, int part_id, hi_dsk_info_t* parent) {
	hi_dsk_info_t* di;
	char dev_path[256];
	char sys_path[256];

	int mode;

	char part_name[32];
	char sys_part_name[32];

	/* Partitions are located in /sys/block/sda/sdaX
	 * while their dev_path is /dev/sdaX, so
	 * they have logic that differs from disks */

	snprintf(part_name, 32, "%s%d", disk_name, part_id);
	snprintf(sys_part_name, 32, "%s/%s", disk_name, part_name);

	mode = hi_linux_make_dev_path(part_name, dev_path);

	hi_dsk_dprintf("hi_linux_disk_proc_partition: Probing %s (%s)\n", dev_path, part_name);

	if(mode == -1) {
		return;
	}

	di = hi_linux_disk_create(part_name, dev_path, sys_part_name);
	di->d_type = HI_DSKT_PARTITION;
	di->d_mode = mode;

	hi_dsk_add(di);
	hi_dsk_attach(di, parent);
}

void hi_linux_disk_proc_disk(const char* name, void* arg) {
	hi_dsk_info_t* di;
	char dev_path[256];

	int mode;

	mode = hi_linux_make_dev_path(name, dev_path);

	hi_dsk_dprintf("hi_linux_disk_proc_partition: Probing %s (%s)\n", dev_path, name);

	if(mode == -1) {
		return;
	}

	di = hi_linux_disk_create(name, dev_path, name);

	if(strncmp(name, "dm", 2) == 0) {
		/* Device-mapper node. All dm-nodes
		 * are considered as volumes until slaves are checked */
		di->d_type = HI_DSKT_VOLUME;
		strncpy(di->d_bus_type, "DM", HIDSKBUSLEN);

		hi_linux_sysfs_readstr(SYS_BLOCK_PATH, name, "dm/uuid", di->d_id, HIDSKIDLEN);
		hi_linux_sysfs_fixstr(di->d_id);
	}
	else if(strncmp(name, "ram", 3) == 0) {
		di->d_type = HI_DSKT_VOLUME;
		strncpy(di->d_bus_type, "RAM", HIDSKBUSLEN);
	}
	else if(strncmp(name, "loop", 4) == 0) {
		di->d_type = HI_DSKT_VOLUME;
		strncpy(di->d_bus_type, "LOOP", HIDSKBUSLEN);
	}
	else {
		int part_range, part_id;

		/* TODO: bus type */
		hi_linux_sysfs_readstr(SYS_BLOCK_PATH, name, "device/model", di->d_model, HIDSKMODELLEN);
		hi_linux_sysfs_fixstr(di->d_model);

		di->d_type = HI_DSKT_DISK;

		/* Search for partitions */
		part_range = (int) hi_linux_sysfs_readuint(SYS_BLOCK_PATH, name, "ext_range", 1);
		for(part_id = 1; part_id < part_range; ++part_id) {
			hi_linux_disk_proc_partition(name, part_id, di);
		}
	}

	hi_dsk_add(di);
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

PLATAPI int hi_dsk_probe(void) {
	int ret;

	ret = hi_linux_sysfs_walk(SYS_BLOCK_PATH, hi_linux_disk_proc_disk, NULL);
	if(ret != HI_LINUX_SYSFS_OK)
		return HI_PROBE_ERROR;

	ret = hi_linux_sysfs_walk(SYS_BLOCK_PATH, hi_linux_disk_proc_slaves, NULL);
	if(ret != HI_LINUX_SYSFS_OK)
		return HI_PROBE_ERROR;

	return HI_PROBE_OK;
}

