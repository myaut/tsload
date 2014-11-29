
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


typedef enum hi_dsk_type {
	HI_DSKT_UNKNOWN,

	HI_DSKT_DISK,
	HI_DSKT_PARTITION,
	HI_DSKT_POOL,
	HI_DSKT_VOLUME
} hi_dsk_type_t;

typedef struct hi_dsk_info {
	hi_object_header_t	d_hdr;

	/* Mandatory fields */
	char* d_path;
	int  d_mode;
	uint64_t d_size;
	hi_dsk_type_t d_type;

	/* Optional fields */
	char* d_bus_type;

	/* For iSCSI LUNs - IQN,
	   For FC LUNs - WWN
	   For SCSI disks - bus/target/LUN
	   For Vol managers - internal ID */
	char* d_port;

	char* d_id;
	char* d_model;
} hi_dsk_info_t;

#define HI_DSK_FROM_OBJ(object)		((hi_dsk_info_t*) (object))

PLATAPI int hi_dsk_probe(void);
void hi_dsk_dtor(hi_object_t* object);
int hi_dsk_init(void);
void hi_dsk_fini(void);

hi_dsk_info_t* hi_dsk_create(void);


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

#endif /* DISKINFO_H_ */

