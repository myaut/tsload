/*
 * modapi.h
 *
 *  Created on: 06.11.2012
 *      Author: myaut
 */

#ifndef MODTSLOAD_H_
#define MODTSLOAD_H_

#include <modules.h>
#include <wlparam.h>
#include <hashmap.h>

#define WLTNAMELEN		64

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
 * @member wlt_name Name of workload type
 * @member wlt_class Bitmask of workload class flags
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
	char		 wlt_name[WLTNAMELEN];

	wl_class_t	wlt_class;

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

int wlt_init(void);
void wlt_fini(void);

#ifndef NO_JSON
#include <libjson.h>
JSONNODE* json_wl_type_format(hm_item_t* object);
#else
#include <tsobj.h>
tsobj_node_t* tsobj_wl_type_format(hm_item_t* object);
#endif

#endif /* MODAPI_H_ */
