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

#define WLTNAMELEN		64

#define WLTHASHSIZE		8
#define WLTHASHMASK		(WLTHASHSIZE - 1)

struct module;
struct workload;
struct request;

typedef int (* wlt_wl_config_func)(struct workload* wl);
typedef int (* wlt_run_request_func)(struct request* wl);

typedef struct wl_type {
	char		 wlt_name[WLTNAMELEN];

	wlp_descr_t* wlt_params;
	size_t 		 wlt_params_size;			/**< sizeof structure where parameter values are stored*/

	wlt_wl_config_func wlt_wl_config;
	wlt_wl_config_func wlt_wl_unconfig;

	wlt_run_request_func wlt_run_request;

	module_t*			wlt_module;
	struct wl_type* 	wlt_next;
} wl_type_t;

LIBEXPORT int wl_type_register(module_t* mod, wl_type_t* wlt);
wl_type_t* wl_type_search(const char* name);

int wlt_init(void);
void wlt_fini(void);

#ifndef NO_JSON
#include <libjson.h>

LIBEXPORT JSONNODE* json_wl_type_format(const char* name);
LIBEXPORT JSONNODE* json_wl_type_format_all();
#endif

#endif /* MODAPI_H_ */
