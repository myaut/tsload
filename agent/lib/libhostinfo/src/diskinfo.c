/*
 * diskinfo.c
 *
 *  Created on: 27.04.2013
 *      Author: myaut
 */

#include <defs.h>
#include <mempool.h>
#include <diskinfo.h>

#include <string.h>

/**
 * diskinfo.c - reads information about disks from operating system
 * */
mp_cache_t hi_dsk_cache;

/**
 * Allocate and initialize disk descriptor hi_dsk_info_t
 * */
hi_dsk_info_t* hi_dsk_create(void) {
	hi_dsk_info_t* di = mp_cache_alloc(&hi_dsk_cache);

	di->d_path[0] = '\0';
	di->d_size = 0;
	di->d_mode = 0;
	di->d_type = HI_DSKT_UNKNOWN;

	di->d_port[0] = '\0';
	di->d_model[0] = '\0';
	di->d_fs[0] = '\0';

	di->d_bus_type[0] = '\0';

	hi_obj_header_init(HI_SUBSYS_DISK, &di->d_hdr, "disk");

	return di;
}

/**
 * Destroy disk descriptor and free slave handles if needed
 * */
void hi_dsk_dtor(hi_object_header_t* object) {
	hi_dsk_info_t* di = (hi_object_t*) object;
	mp_cache_free(&hi_dsk_cache, di);
}


int hi_dsk_init(void) {
	mp_cache_init(&hi_dsk_cache, hi_dsk_info_t);

	return 0;
}

void hi_dsk_fini(void) {
	mp_cache_destroy(&hi_dsk_cache);
}

const char* tsobj_hi_dsk_format_class(hi_dsk_info_t* di) {
	switch(di->d_type) {
	case HI_DSKT_DISK:
		return "tsload.hi.Disk";
	case HI_DSKT_PARTITION:
		return "tsload.hi.DiskPartition";
	case HI_DSKT_POOL:
		return "tsload.hi.DiskPool";
	case HI_DSKT_VOLUME:
		return "tsload.hi.Volume";
	}

	return "tsload.hi.DiskObject";
}

tsobj_node_t* tsobj_hi_dsk_format(struct hi_object_header* obj) {
	hi_dsk_info_t* di = HI_DSK_FROM_OBJ(obj);
	tsobj_node_t* dsk = tsobj_new_node(tsobj_hi_dsk_format_class(di));

	tsobj_add_string(dsk, TSOBJ_STR("path"), tsobj_str_create(di->d_path));

	tsobj_add_integer(dsk, TSOBJ_STR("size"), di->d_size);
	tsobj_add_integer(dsk, TSOBJ_STR("mode"), di->d_mode);

	tsobj_add_string(dsk, TSOBJ_STR("bus_type"), tsobj_str_create(di->d_bus_type));
	tsobj_add_string(dsk, TSOBJ_STR("model"), tsobj_str_create(di->d_model));
	tsobj_add_string(dsk, TSOBJ_STR("port"), tsobj_str_create(di->d_port));

	tsobj_add_string(dsk, TSOBJ_STR("fs"), tsobj_str_create(di->d_fs));

	return dsk;
}
