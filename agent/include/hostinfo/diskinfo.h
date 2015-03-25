
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



#ifndef DISKINFO_H_
#define DISKINFO_H_

#include <tsload/defs.h>

#include <tsload/list.h>

#include <hostinfo/hiobject.h>

#include <string.h>

/**
 * @module DiskInfo
 * 
 * DiskInfo retrives information on block devices available on system
 * including Hard Disks and partitions on them, and Volume Manager objects
 * 
 * Due to complex logic of Disk Hierarchy and huge number of Volume Managers 
 * (which of them of course has own logic on hierarchy), DiskInfo may look
 * inconsistent, but it is best what we can do.
 * 
 * __NOTE__: Currently if pool or volume reside on partition, than partition 
 * would be considered a child to that pool or volume. It also would be child 
 * to disk drive. This rule may change in future releases of HostInfo.
 * 
 * Disk object children are sometimes called "slaves" due to Linux SYSFS naming
 * scheme.
 */

/**
 * DiskInfo object type
 * 
 * @value HI_DSKT_UNKNOWN	unknown object (shouldn't be exposed, used internally)
 * @value HI_DSKT_DISK		real or virtual disk drive
 * @value HI_DSKT_PARTITION	partition on disk drive
 * @value HI_DSKT_POOL		Volume Manager "pool" -- collection of disks, partitions 	\
 *                          or whatever. Note that pools haven't real block device		\
 *                          corresponding to it, it is used only to show hierarchy
 * @value HI_DSKT_VOLUME	Logical Volume that resiedes on disks, partitions or inside pools
 */
typedef enum hi_dsk_type {
	HI_DSKT_UNKNOWN,

	HI_DSKT_DISK,
	HI_DSKT_PARTITION,
	HI_DSKT_POOL,
	HI_DSKT_VOLUME
} hi_dsk_type_t;

/**
 * DiskInfo descriptor
 * 
 * @member d_hdr 		HiObject header 
 * @member d_disk_name	Unique name of disk device. For volume managers symbolic name may	\
 * 						be namespaced, i.e. md:d10 or zfs:volume to ensure uniqueness		\
 * 						Same as `d_hdr.name`
 * @member d_path		Path to corresponding block device. For devices those not support 	\
 * 						direct access, this field may contain some meaningful path, which	\
 * 						however couldn't be argument to `open()` or `CreateFile()`. See		\
 * 						platform notes for details.
 * @member d_mode		Access permissions to that disk object.
 * @member d_size		Size of disk, volume, pool or partition in bytes
 * @member d_type		Type of that descriptor
 * @member d_bus_type	(optional) Identifies how this disk plugged into hierarchy. May be 	\
 * 						name of the bus or a driver handling that bus or contain volume 	\
 * 						manager name along with volume manager device subtype
 * @member d_port		(optional) Optional identifier of hardware port where disk is 		\
 * 						plugged in. Usually it is SCSI host, SCSI target and SCSI LUN
 * @member d_id			(optional) Identifier provided by lower layer
 * @member d_model		(optional) For disk drives - vendor and model of disk
 */
typedef struct hi_dsk_info {
	hi_object_header_t	d_hdr;
#define d_disk_name		d_hdr.name

	/* Mandatory fields */
	AUTOSTRING char* d_path;
	int  d_mode;
	uint64_t d_size;
	hi_dsk_type_t d_type;

	/* Optional fields */
	AUTOSTRING char* d_bus_type;
	AUTOSTRING char* d_port;
	AUTOSTRING char* d_id;
	AUTOSTRING char* d_model;
} hi_dsk_info_t;

/**
 * Conversion macros
 */
#define HI_DSK_FROM_OBJ(object)		((hi_dsk_info_t*) (object))
#define HI_DSK_TO_OBJ(di)			(&di->d_hdr)

PLATAPI int hi_dsk_probe(void);
void hi_dsk_dtor(hi_object_t* object);
int hi_dsk_init(void);
void hi_dsk_fini(void);

hi_dsk_info_t* hi_dsk_create(void);

LIBEXPORT hi_object_t* hi_dsk_check_overlap(hi_dsk_info_t* di);


/**
 * Add disk descriptor to global list */
STATIC_INLINE void hi_dsk_add(hi_dsk_info_t* di) {
	hi_obj_add(HI_SUBSYS_DISK, (hi_object_header_t*) &di->d_hdr);
}

/**
 * Attach disk descriptor to parent as a slave
 *
 * @param di		Slave disk descriptor
 * @param parent    Parent disk descriptor
 */
STATIC_INLINE void hi_dsk_attach(hi_dsk_info_t* di, hi_dsk_info_t* parent) {
	hi_obj_attach((hi_object_header_t*) &di->d_hdr,
				  (hi_object_header_t*) &parent->d_hdr);
}

/**
 * Find disk by it's name
 * @param name - name of disk
 *
 * @return disk descriptor or NULL if it wasn't found
 * */
STATIC_INLINE hi_dsk_info_t* hi_dsk_find(const char* name) {
	return (hi_dsk_info_t*)	hi_obj_find(HI_SUBSYS_DISK, name);
}

/**
 * Probe system's disk (if needed) and return pointer
 * to global disk descriptor list head
 *
 * @param reprobe Probe system's disks again
 *
 * @return pointer to head or NULL if probe failed
 */
STATIC_INLINE list_head_t* hi_dsk_list(boolean_t reprobe) {
	return hi_obj_list(HI_SUBSYS_DISK, reprobe);
}

tsobj_node_t* tsobj_hi_dsk_format(struct hi_object_header* obj);

PLATAPI int plat_hi_dsk_init(void);
PLATAPI void plat_hi_dsk_fini(void);

#endif /* DISKINFO_H_ */

