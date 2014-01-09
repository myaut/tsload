/*
 * modules.h
 *
 *  Created on: 06.11.2012
 *      Author: myaut
 */

#ifndef MODULES_H_
#define MODULES_H_

#include <modapi.h>

#include <defs.h>
#include <plat/modules.h>

#define MODPATHLEN	256
#define MODSTATUSMSG 512

/**
 * @module Modules
 *
 * API for working with plugins.
 *
 * mod_init() walks mod_search_path, loads every shared file, checks if it is
 * compliant with 'modapi' and runs mod_config() from it.
 *
 * mod_fini() calls mod_unconfig() for each loaded module and unmaps them
 *
 *
 * Module states
 *
 * ```
 *                       +----------> CONFIG_ERROR
 * 						 |
 * UNITIALIZED ---> UNCONFIGURED ---> READY ---> REQUEST_ERROR
 *                                     ^                |
 *                                     +----------------+
 * ```
 *
 * @note Modules are not signed, so do not play with permissions to module path
 */

typedef enum mod_status {
	MOD_UNITIALIZED,
	MOD_UNCONFIGURED,
	MOD_READY,
	MOD_CONFIG_ERROR,
	MOD_REQUEST_ERROR
} mod_status_t;

/**
 * Module main structure
 *
 * @member mod_path		Path to module file (realpath)
 * @member mod_name		Internal module name
 * @member mod_status_msg Status message reported by module
 * @member mod_private  Field available to module for it's internals
 */
typedef struct module {
	char mod_path[MODPATHLEN];
	plat_mod_library_t mod_library;

	char* mod_name;

	mod_status_t mod_status;
	char* mod_status_msg;

	mod_config_func mod_config;
	mod_config_func mod_unconfig;

	void* mod_private;

	struct module* mod_next;
} module_t;

LIBEXPORT module_t* mod_search(const char* name);
LIBEXPORT int mod_error(module_t* mod, char* fmtstr, ...);

LIBEXPORT void* mod_load_symbol(module_t* mod, const char* name);

LIBEXPORT module_t* mod_get_first();

#define MOD_LOAD_SYMBOL(dest, mod, name, flag) 					   		   \
	   do { void* sym = mod_load_symbol(mod, name); 				   	   \
			if(sym == NULL)	{			 								   \
				logmsg(LOG_WARN, "Required symbol %s is undefined", name); \
				flag = B_TRUE;			 								   \
			}							 								   \
			dest = sym; } while(0)

/* Platform-dependent functions */
PLATAPI int plat_mod_open(plat_mod_library_t* lib, const char* path);
PLATAPI int plat_mod_close(plat_mod_library_t* lib);
PLATAPI void* plat_mod_load_symbol(plat_mod_library_t* lib, const char* name);
PLATAPI char* plat_mod_error_msg(void);

LIBEXPORT int mod_init(void);
LIBEXPORT void mod_fini(void);

#endif /* MODULES_H_ */
