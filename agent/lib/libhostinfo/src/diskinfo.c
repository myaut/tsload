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
