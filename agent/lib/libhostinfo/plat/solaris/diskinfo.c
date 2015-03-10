
/*
    This file is part of TSLoad.
    Copyright 2013-2014, Sergey Klyaus, ITMO University

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

#include <tsload/mempool.h>
#include <tsload/pathutil.h>
#include <tsload/autostring.h>

#include <hostinfo/diskinfo.h>

#include <hitrace.h>

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <ctype.h>

/* In Solaris list_node_t already exists */
#define list_node_t		__sol_list_node_t
#define list_node		__sol_list_node
#define list_move_tail	__sol_list_move_tail

#include <libdiskmgt.h>

#undef list_move_tail
#undef list_node_t
#undef list_node

/**
 * diskinfo (Solaris)
 *
 * Uses libdiskmgt to enumerate disks/partitions
 *
 * TODO: access rights check
 * TODO: SVM
 * TODO: d_port identification
 * */

int hi_zfs_probe(void);

#define HI_SOL_DSK_MEDIA		0x2
#define HI_SOL_DSK_ALIASES		0x4
#define HI_SOL_DSK_CONTROLLER	0x8

#define HI_SOL_DSK_INVALID	ULONG_MAX

/* diskmgt doesn't provide sector_size
 * needed to calculate partition size */
#define SECTOR_SIZE			512

/* diskmgt disk descriptor
 * diskmgt provides information in sets of dm_descriptor_t objects
 * and nvlist_t attributes which is very uncomfortable.
 *
 * Instead of using them in code, we read disk information
 * into this structure and keep it through  */
typedef struct {
	dm_descriptor_t sd_disk;

	dm_descriptor_t* sd_media_arr;
	dm_descriptor_t  sd_media;

	dm_descriptor_t* sd_aliases_arr;

	dm_descriptor_t* sd_ctlr_arr;

	nvlist_t* sd_disk_attrs;
	nvlist_t* sd_media_attrs;

	char* sd_opath;
	char* sd_vendor_id;
	char* sd_product_id;

	char* sd_ctype;

	uint64_t sd_size;

	uint32_t sd_nheads;
	uint32_t sd_nsectors;

	uint32_t sd_target;
	uint32_t sd_lun;
	char 	 sd_wwn[32];

	int sd_flags;
} hi_sol_dsk_t;

/**
 * Small set of libdiskmgmt adapters that allow to make
 * code cleaner. Instead of checking error after each
 * operation, composes them into bigger subsets.
 * */

/* Read disk attributes:
 * DM_OPATH - path to disk
 * DM_PRODUCT_ID/DM_VENDOR_ID - disk model */
static
int hi_sol_dm_get_attrs(hi_sol_dsk_t* dsk) {
	int error = 0;

	dsk->sd_disk_attrs = dm_get_attributes(dsk->sd_disk, &error);

	if(error != 0)
		return error;

	if(nvlist_lookup_string(dsk->sd_disk_attrs, DM_OPATH, &dsk->sd_opath) != 0)
		return -1;
	if(nvlist_lookup_string(dsk->sd_disk_attrs, DM_PRODUCT_ID, &dsk->sd_product_id) != 0)
		dsk->sd_product_id = "";
	if(nvlist_lookup_string(dsk->sd_disk_attrs, DM_VENDOR_ID, &dsk->sd_vendor_id) != 0)
		dsk->sd_vendor_id = "";

	return error;
}

static
int hi_sol_dm_get_media(hi_sol_dsk_t* dsk) {
	int error = 0;

	dsk->sd_media_arr = dm_get_associated_descriptors(dsk->sd_disk, DM_MEDIA, &error);

	if(error != 0)
		return error;
	if(dsk->sd_media_arr == NULL || dsk->sd_media_arr[0] == 0)
		return -1;

	dsk->sd_flags |= HI_SOL_DSK_MEDIA;

	dsk->sd_media = dsk->sd_media_arr[0];
	dsk->sd_media_attrs = dm_get_attributes(dsk->sd_media, &error);

	if(nvlist_lookup_uint64(dsk->sd_media_attrs, DM_SIZE, &dsk->sd_size) != 0)
		return -1;
	if(nvlist_lookup_uint32(dsk->sd_media_attrs, DM_NHEADS, &dsk->sd_nheads) != 0)
		return -1;
	if(nvlist_lookup_uint32(dsk->sd_media_attrs, DM_NSECTORS, &dsk->sd_nsectors) != 0)
		return -1;

	return error;
}

static
int hi_sol_dm_get_controller(hi_sol_dsk_t* dsk) {
	int error = 0;
	nvlist_t* attrs;

	dsk->sd_ctlr_arr = dm_get_associated_descriptors(dsk->sd_disk, DM_CONTROLLER, &error);

	if(error != 0)
		return error;
	if(dsk->sd_ctlr_arr == NULL || dsk->sd_ctlr_arr[0] == 0)
		return -1;

	dsk->sd_flags |= HI_SOL_DSK_CONTROLLER;

	attrs = dm_get_attributes(dsk->sd_ctlr_arr[0], &error);
	if(error != 0)
		return error;
	if(attrs == NULL)
		return -1;

	if(nvlist_lookup_string(attrs, DM_CTYPE, &dsk->sd_ctype) != 0)
		return -1;

	return error;
}

static
int hi_sol_dm_get_aliases(hi_sol_dsk_t* dsk) {
	int error = 0;

	dsk->sd_aliases_arr = dm_get_associated_descriptors(dsk->sd_disk, DM_ALIAS, &error);

	if(error != 0)
		return error;
	if(dsk->sd_aliases_arr == NULL || dsk->sd_aliases_arr[0] == 0)
		return -1;

	dsk->sd_flags |= HI_SOL_DSK_ALIASES;

	return error;
}

static
void hi_sol_dsk_init(hi_sol_dsk_t* dsk, dm_descriptor_t disk) {
	dsk->sd_disk = disk;
	dsk->sd_media_arr = NULL;
	dsk->sd_flags = 0;

	dsk->sd_target = HI_SOL_DSK_INVALID;
	dsk->sd_lun    = HI_SOL_DSK_INVALID;
	dsk->sd_wwn[0] = '\0';
}

static
void hi_sol_dsk_cleanup(hi_sol_dsk_t* dsk) {
	if(dsk->sd_flags & HI_SOL_DSK_MEDIA)
		dm_free_descriptors(dsk->sd_media_arr);

	if(dsk->sd_flags & HI_SOL_DSK_ALIASES)
		dm_free_descriptors(dsk->sd_aliases_arr);

	if(dsk->sd_flags & HI_SOL_DSK_CONTROLLER)
		dm_free_descriptors(dsk->sd_ctlr_arr);
}

static
int hi_sol_dm_read_aliases(hi_sol_dsk_t* dsk) {
	nvlist_t* attrs;
	int error;

	int i;

	char* wwn = NULL;
	uint32_t target;
	uint32_t lun;

	boolean_t wwn_set = B_FALSE;
	boolean_t target_set = B_FALSE;
	boolean_t lun_set = B_FALSE;

	for(i = 0; dsk->sd_aliases_arr[i] != 0; ++i) {
		attrs = dm_get_attributes(dsk->sd_aliases_arr[i], &error);

		if(error != 0)
			return error;
		if(attrs == NULL)
			return -1;

		if(!wwn_set && nvlist_lookup_string(attrs, DM_WWN, &wwn) == 0) {
			if(wwn != NULL) {
				strncpy(dsk->sd_wwn, wwn, 32);
				wwn_set = B_TRUE;
			}
		}

		if(!target_set && nvlist_lookup_uint32(attrs, DM_TARGET, &target) == 0) {
			dsk->sd_target = target;
			target_set = B_TRUE;
		}

		if(!lun_set && nvlist_lookup_uint32(attrs, DM_LUN, &lun) == 0) {
			dsk->sd_lun = lun;
			lun_set = B_TRUE;
		}
	}

	hi_dsk_dprintf("hi_sol_dm_read_aliases: found %d aliases; wwn: %c target: %c lun: %c\n",
						i, wwn_set? '+' : '-', target_set? '+' : '-', lun_set? '+' : '-');

	return 0;
}

/**
 * Walk over ddp list of DM descriptors, and call proc function for each descriptor
 * Useful for slices/partitions walking
 *
 * @param parent	parent node where slice/partition would be attached
 * @param dsk       global disk structure
 * @param ddp       dm descriptor array
 * @param proc      processor function
 * */
void hi_sol_dm_walk(hi_dsk_info_t* parent, hi_sol_dsk_t* dsk, dm_descriptor_t* ddp,
				 void (*proc)(hi_dsk_info_t* parent, hi_sol_dsk_t* dsk, char* name, dm_descriptor_t dd)) {
	int error = 0;
	int i = 0;
	char* name;
	dm_descriptor_t dd;

	for(i = 0; ddp[i] != 0; ++i) {
		dd = ddp[i];
		name = dm_get_name(dd, &error);

		/* FIXME: For flash drives libdiskmgt provides name == NULL, need to investigate */
		if(error != 0 || name == NULL)
			continue;

		proc(parent, dsk, name, dd);

		dm_free_name(name);
	}
}

void hi_sol_proc_slice(hi_dsk_info_t* parent, hi_sol_dsk_t* dsk, char* slice_name, dm_descriptor_t slice) {
	int error;
	hi_dsk_info_t* di;

	path_split_iter_t psi;

	uint64_t size;
	nvlist_t* attrs = dm_get_attributes(slice, &error);

	if(error != 0)
		return;
	if(nvlist_lookup_uint64(attrs, DM_SIZE, &size) != 0)
		return;

	hi_dsk_dprintf("hi_sol_proc_slice: slice %s size: %llu\n", slice_name, size);

	di = hi_dsk_create();

	aas_copy(&di->d_hdr.name, path_basename(&psi, slice_name));
	aas_copy(&di->d_path, slice_name);

	di->d_size = size * SECTOR_SIZE;
	di->d_type = HI_DSKT_PARTITION;		/* Slices considered as partitions */

	hi_dsk_add(di);
	hi_dsk_attach(di, parent);
}

void hi_sol_proc_slices(hi_dsk_info_t* parent, hi_sol_dsk_t* dsk, dm_descriptor_t dd) {
	int error;
	dm_descriptor_t* slices = dm_get_associated_descriptors(dd, DM_SLICE, &error);

	if(error != 0 || slices == NULL)
		return;

	hi_sol_dm_walk(parent, dsk, slices, hi_sol_proc_slice);
}

void hi_sol_proc_partition(hi_dsk_info_t* parent, hi_sol_dsk_t* dsk, char* part_name, dm_descriptor_t part) {
	int error;
	hi_dsk_info_t* di;

	uint32_t bcyl, bhead, bsect;
	uint32_t ecyl, ehead, esect;
	uint64_t size;

	path_split_iter_t psi;

	nvlist_t* attrs = dm_get_attributes(part, &error);

	if(error != 0)
		return;

	if(nvlist_lookup_uint32(attrs, DM_BCYL, &bcyl) != 0)
		return;
	if(nvlist_lookup_uint32(attrs, DM_BHEAD, &bhead) != 0)
		return;
	if(nvlist_lookup_uint32(attrs, DM_BSECT, &bsect) != 0)
		return;

	if(nvlist_lookup_uint32(attrs, DM_ECYL, &ecyl) != 0)
		return;
	if(nvlist_lookup_uint32(attrs, DM_EHEAD, &ehead) != 0)
		return;
	if(nvlist_lookup_uint32(attrs, DM_ESECT, &esect) != 0)
		return;

	/* XXX: Is this correct? */
	size = (esect + (ehead + ecyl * dsk->sd_nheads) * dsk->sd_nsectors) -
		   (bsect + (bhead + bcyl * dsk->sd_nheads) * dsk->sd_nsectors);
	size *= SECTOR_SIZE;

	hi_dsk_dprintf("hi_sol_proc_partition: partition %s size: %llu\n", part_name, size);
	hi_dsk_dprintf("hi_sol_proc_partition: bcyl: %8d bhead: %8d bsect: %8d\n", bcyl, bhead, bsect);
	hi_dsk_dprintf("hi_sol_proc_partition: ecyl: %8d ehead: %8d esect: %8d\n", ecyl, ehead, esect);
	hi_dsk_dprintf("hi_sol_proc_partition: nheads: %d nsectors: %d \n", dsk->sd_nheads, dsk->sd_nsectors);

	di = hi_dsk_create();

	aas_copy(&di->d_hdr.name, path_basename(&psi, part_name));
	aas_copy(&di->d_path, part_name);

	di->d_size = size;
	di->d_type = HI_DSKT_PARTITION;

	hi_dsk_add(di);
	hi_dsk_attach(di, parent);

	hi_sol_proc_slices(di, dsk, part);
}

void hi_sol_proc_partitions(hi_dsk_info_t* parent, hi_sol_dsk_t* dsk) {
	int error;
	dm_descriptor_t* partitions = dm_get_associated_descriptors(dsk->sd_media, DM_PARTITION, &error);

	if(error != 0 || partitions == NULL)
		return;

	hi_sol_dm_walk(parent, dsk, partitions, hi_sol_proc_partition);
}

void hi_sol_set_disk_name(hi_dsk_info_t* di, const char* opath) {
	path_split_iter_t iter;
	const char* basename = path_basename(&iter, opath);
	
	char* dskname = mp_malloc(strlen(basename) + 1);
	
	const char* p = basename;
	const char* x;
	char* q = dskname;
	boolean_t flag = B_TRUE;
	
	/* Cut out sX or pX part from COTODOSO name for disk instance names
	 * because it confuses ZFS and not consistent (dup name!) */
	while(*p && flag) {
		*q++ = *p++;
		
		if(*p == 's' || *p == 'p') {
			/* Check if the rest are digits */
			x = p + 1;
			
			flag = B_FALSE;
			while(*x) {
				if(!isdigit(*x++)) {
					/* Continue searching */
					flag = B_TRUE;
					break;
				}
			}
		}
	}
	
	*q++ = '\0';
	
	aas_copy(&di->d_disk_name, dskname);
	mp_free(dskname);
}

void hi_sol_proc_disk(hi_dsk_info_t* parent, hi_sol_dsk_t* pdsk, char* name, dm_descriptor_t disk) {
	hi_sol_dsk_t dsk;
	hi_dsk_info_t* di;

	hi_sol_dsk_init(&dsk, disk);

	hi_dsk_dprintf("hi_sol_proc_disk: Processing disk %s (%llx)\n", name, disk);

	/* Read information about disk from libdiskmgt */
	if(hi_sol_dm_get_attrs(&dsk) != 0)
		goto error;
	if(hi_sol_dm_get_aliases(&dsk) != 0)
		goto error;
	if(hi_sol_dm_get_media(&dsk) != 0)
		goto error;
	if(hi_sol_dm_get_controller(&dsk) != 0)
		goto error;
	if(hi_sol_dm_read_aliases(&dsk) != 0)
		goto error;

	/* Create disk instance */
	di = hi_dsk_create();
	hi_sol_set_disk_name(di, dsk.sd_opath);
	
	aas_copy(&di->d_path, dsk.sd_opath);

	di->d_size = dsk.sd_size * SECTOR_SIZE;
	di->d_type = HI_DSKT_DISK;

	aas_copy(&di->d_bus_type, dsk.sd_ctype);
	aas_copy(&di->d_id, name);

	aas_printf(&di->d_model, "%s %s", dsk.sd_vendor_id, dsk.sd_product_id);

	hi_dsk_add(di);

	hi_sol_proc_partitions(di, &dsk);
	hi_sol_proc_slices(di, &dsk, dsk.sd_media);

error:
	hi_sol_dsk_cleanup(&dsk);
}

PLATAPI int hi_dsk_probe(void) {
	int error;

	dm_descriptor_t	*ddp;

	ddp = dm_get_descriptors(DM_DRIVE, NULL, &error);

	if(error != 0)
		return HI_PROBE_ERROR;

	if(ddp == NULL) {
		/* No disks found */
		return HI_PROBE_OK;
	}

	hi_sol_dm_walk(NULL, NULL, ddp, hi_sol_proc_disk);

	dm_free_descriptors(ddp);
	
	error = hi_zfs_probe();
	
	return error;
}

