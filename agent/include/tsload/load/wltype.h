
/*
    This file is part of TSLoad.
    Copyright 2012-2014, Sergey Klyaus, ITMO University

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



#ifndef WLTYPE_H_
#define WLTYPE_H_

#include <tsload/defs.h>

#include <tsload/hashmap.h>
#include <tsload/autostring.h>
#include <tsload/modules.h>

#include <tsload/obj/obj.h>

#include <tsload/load/wlparam.h>


#define WLTHASHSIZE		8
#define WLTHASHMASK		(WLTHASHSIZE - 1)

struct module;
struct workload;
struct request;
struct workload_step;

/**
 * @module Workload types
 */

typedef int (* wlt_wl_config_func)(struct workload* wl);
typedef int (* wlt_wl_step_func)(struct workload_step* wls);
typedef int (* wlt_run_request_func)(struct request* wl);

/**
 * Workload classes
 */
typedef enum wl_class {
	WLC_CPU_INTEGER 		= 0x0001,
	WLC_CPU_FLOAT			= 0x0002,
	WLC_CPU_MEMORY			= 0x0004,
	WLC_CPU_MISC			= 0x0008,

	WLC_MEMORY_ALLOCATION 	= 0x0010,

	WLC_FILESYSTEM_OP		= 0x0100,
	WLC_FILESYSTEM_RW		= 0x0200,
	WLC_DISK_RW				= 0x0400,

	WLC_NETWORK				= 0x1000,

	WLC_OS_BENCHMARK		= 0x10000,

	WLC_NET_CLIENT			= 0x100000
} wl_class_t;

/**
 * Workload type descriptor
 *
 * Set up it statically in your module
 *
 * @member wlt_name Name of workload type (use AAS_CONST_STR for it)
 * @member wlt_class Bitmask of workload class flags
 * @member wlt_description Description of a workload type
 * @member wlt_params pointer to a vector of workload/request parameter descriptors
 * @member wlt_params_size sizeof() of workload parameters structure
 * @member wlt_rqparams_size sizeof() of request parameters structure
 * @member wlt_wl_config pointer to function that configures workload
 * @member wlt_wl_unconfig pointer to function that destroyes worklaod
 * @member wlt_wl_step function that will be called at beginning of each step
 * @member wlt_run_request primary workload function that services request
 * @member wlt_module should be set to module structure passed to mod_config()
 * @member wlt_next field internally used by TSLoad, set to NULL
 */
typedef struct wl_type {
	AUTOSTRING char* wlt_name;

	wl_class_t	wlt_class;
	
	const char* wlt_description;

	wlp_descr_t* wlt_params;
	size_t 		 wlt_params_size;
	size_t		 wlt_rqparams_size;

	wlt_wl_config_func   wlt_wl_config;
	wlt_wl_config_func   wlt_wl_unconfig;

	wlt_wl_step_func 	 wlt_wl_step;

	wlt_run_request_func wlt_run_request;

	module_t*			wlt_module;
	struct wl_type* 	wlt_next;
} wl_type_t;

LIBEXPORT int wl_type_register(module_t* mod, wl_type_t* wlt);
LIBEXPORT int wl_type_unregister(module_t* mod, wl_type_t* wlt);
wl_type_t* wl_type_search(const char* name);

LIBEXPORT int wlt_init(void);
LIBEXPORT void wlt_fini(void);

tsobj_node_t* tsobj_wl_type_format(hm_item_t* object);

#endif /* WLTYPE_H_ */

