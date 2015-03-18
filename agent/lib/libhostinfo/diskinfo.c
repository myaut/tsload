
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

#include <tsload/mempool.h>
#include <tsload/autostring.h>

#include <hostinfo/diskinfo.h>

#include <string.h>


mp_cache_t hi_dsk_cache;

/**
 * Allocate and initialize disk descriptor hi_dsk_info_t
 * */
hi_dsk_info_t* hi_dsk_create(void) {
	hi_dsk_info_t* di = mp_cache_alloc(&hi_dsk_cache);

	aas_init(&di->d_path);
	di->d_size = 0;
	di->d_mode = 0;
	di->d_type = HI_DSKT_UNKNOWN;

	aas_init(&di->d_port);
	aas_init(&di->d_model);
	aas_init(&di->d_bus_type);
	aas_init(&di->d_id);

	hi_obj_header_init(HI_SUBSYS_DISK, &di->d_hdr, NULL);

	return di;
}

/**
 * Destroy disk descriptor and free slave handles if needed
 * */
void hi_dsk_dtor(hi_object_header_t* object) {
	hi_dsk_info_t* di = (hi_object_t*) object;

	aas_free(&di->d_path);
	aas_free(&di->d_port);
	aas_free(&di->d_model);
	aas_free(&di->d_bus_type);
	aas_free(&di->d_id);

	mp_cache_free(&hi_dsk_cache, di);
}


int hi_dsk_init(void) {
	mp_cache_init(&hi_dsk_cache, hi_dsk_info_t);

	return plat_hi_dsk_init();
}

void hi_dsk_fini(void) {
	plat_hi_dsk_fini();
	
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

	if(di->d_bus_type)
		tsobj_add_string(dsk, TSOBJ_STR("bus_type"), tsobj_str_create(di->d_bus_type));
	if(di->d_model)
		tsobj_add_string(dsk, TSOBJ_STR("model"), tsobj_str_create(di->d_model));
	if(di->d_port)
		tsobj_add_string(dsk, TSOBJ_STR("port"), tsobj_str_create(di->d_port));

	return dsk;
}

