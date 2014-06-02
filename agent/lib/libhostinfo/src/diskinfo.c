
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

const char* json_hi_dsk_format_type(struct hi_object_header* obj) {
	hi_dsk_info_t* di = HI_DSK_FROM_OBJ(obj);

	switch(di->d_type) {
	case HI_DSKT_DISK:
		return "disk";
	case HI_DSKT_PARTITION:
		return "partition";
	case HI_DSKT_POOL:
		return "pool";
	case HI_DSKT_VOLUME:
		return "volume";
	}

	return "unknown";
}

JSONNODE* json_hi_dsk_format(struct hi_object_header* obj) {
	JSONNODE* jdsk = json_new(JSON_NODE);
	hi_dsk_info_t* di = HI_DSK_FROM_OBJ(obj);

	json_push_back(jdsk, json_new_a("path", di->d_path));

	json_push_back(jdsk, json_new_i("size", di->d_size));
	json_push_back(jdsk, json_new_i("mode", di->d_mode));

	json_push_back(jdsk, json_new_a("bus_type", di->d_bus_type));
	json_push_back(jdsk, json_new_a("model", di->d_model));
	json_push_back(jdsk, json_new_a("port", di->d_port));

	json_push_back(jdsk, json_new_a("fs", di->d_fs));

	return jdsk;
}

