
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


#include <tsload/defs.h>

#define LOG_SOURCE "modules"
#include <tsload/log.h>

#include <tsload/autostring.h>
#include <tsload/mempool.h>
#include <tsload/modules.h>
#include <tsload/dirent.h>
#include <tsload/pathutil.h>
#include <tsload/readlink.h>

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


/**
 * Module search path
 */
static char* mod_search_path;

mp_cache_t mod_cache;

/* First module in modules linked list
 * TODO: Replace with hash_map? */
module_t* first_module = NULL;

int mod_type = MOD_TSLOAD;

module_t* mod_load(const char* path_name);
void mod_destroy(module_t* mod);

static int load_modules() {
	plat_dir_t* dir = plat_opendir(mod_search_path);
	plat_dir_entry_t* d = NULL;
	char path[MODPATHLEN];
	char link[MODPATHLEN];
	char dst[MODPATHLEN];

	if(!dir) {
		logmsg(LOG_CRIT, "Failed to open directory %s", mod_search_path);
		logerror();

		return -1;
	}

	while((d = plat_readdir(dir)) != NULL) {
		if(plat_dirent_hidden(d))
			continue;

		if(plat_dirent_type(d) == DET_REG) {
			path_join(path, MODPATHLEN, mod_search_path, d->d_name, NULL);
		}
		else if(plat_dirent_type(d) == DET_SYMLINK) {
			path_join(link, MODPATHLEN, mod_search_path, d->d_name, NULL);
			plat_readlink(link, dst, MODPATHLEN);
			path_join(path, MODPATHLEN, mod_search_path, dst, NULL);

			/* FIXME: should normalize path here */
		}
		else {
			continue;
		}

		mod_load(path);
	}

	plat_closedir(dir);
	return 0;
}

static void unload_modules(void) {
	module_t* mod = first_module;
	module_t* next;

	while(mod != NULL) {
		next = mod->mod_next;
		mod_destroy(mod);
		mod = next;
	}
}

module_t* mod_create() {
	module_t* mod = (module_t*) mp_malloc(sizeof(module_t));

	mod->mod_status = MOD_UNITIALIZED;
	aas_init(&mod->mod_status_msg);
	
	mod->mod_next = NULL;

	return mod;
}

/*Add module to modules linked list*/
void mod_add(module_t* mod) {
	if(!first_module) {
		first_module = mod;
	}
	else {
		/*Search for list tail*/
		module_t* mi = first_module;

		while(mi->mod_next)
			mi = mi->mod_next;

		mi->mod_next = mod;
	}
}

void mod_destroy(module_t* mod) {
	int err;
	/*It doesn't unlink module from linked list because this function is called during cleanup*/
	/*TODO: cleanup other fields like mod_private*/

	aas_free(&mod->mod_status_msg);
	
	if(mod->mod_status == MOD_READY) {
		mod->mod_unconfig(mod);
	}

	if(mod->mod_status != MOD_UNITIALIZED) {
		if((err = plat_mod_close(&mod->mod_library)) != 0) {
			logmsg(LOG_WARN, "Failed to close module %s. platform-specific error code: %d",
					mod->mod_path, err);
		}

		logmsg(LOG_INFO, "Destroying module %s", mod->mod_path);
	}

	mp_free(mod);
}

/**
 * Search module by it's name
 *
 * @return pointer to module or NULL if it was not found
 */
module_t* mod_search(const char* name) {
	module_t* mod = first_module;

	while(mod != NULL) {
		if(strcmp(mod->mod_name, name) == 0)
			break;

		mod = mod->mod_next;
	}

	return mod;
}

module_t* mod_get_first() {
	return first_module;
}

void* mod_load_symbol(module_t* mod, const char* name) {
	return plat_mod_load_symbol(&mod->mod_library, name);
}

module_t* mod_load(const char* path_name) {
	module_t* mod = mod_create();
	int* api_version;
	int* type;
	int err = 0;

	boolean_t flag = B_FALSE;

	assert(mod != NULL);

	logmsg(LOG_INFO, "Loading module %s ...", path_name);

	err = plat_mod_open(&mod->mod_library, path_name);

	if(err != 0) {
		logmsg(LOG_WARN, "Failed loading, platform-specific error code: %d [%s]",
				err, plat_mod_error_msg());
		logerror();

		goto fail;
	}

	strcpy(mod->mod_path, path_name);

	/*Load module api version and name*/

	MOD_LOAD_SYMBOL(api_version, mod, "mod_api_version", flag);
	MOD_LOAD_SYMBOL(type, mod, "mod_type", flag);
	MOD_LOAD_SYMBOL(mod->mod_name, mod, "mod_name", flag);

	if(flag)
		goto fail;

	if(*type != mod_type) {
		logmsg(LOG_INFO, "Ignoring module - wrong type %d", *type);

		goto fail;
	}

	if(*api_version != MOD_API_VERSION) {
		logmsg(LOG_WARN, "Wrong api version %d", *api_version);

		goto fail;
	}

	if(mod_search(mod->mod_name) != NULL) {
		logmsg(LOG_WARN, "Module %s already exists", mod->mod_name);

		goto fail;
	}

	MOD_LOAD_SYMBOL(mod->mod_config, mod, "mod_config", flag);
	MOD_LOAD_SYMBOL(mod->mod_unconfig, mod, "mod_unconfig", flag);

	/*Call helper*/
	mod->mod_status = MOD_UNCONFIGURED;

	if(mod->mod_config(mod) != MOD_OK) {
		logmsg(LOG_INFO, "Failed to configure module %s (path: %s)", mod->mod_name, path_name);

		goto fail;
	}

	logmsg(LOG_INFO, "Loaded module %s (path: %s)", mod->mod_name, path_name);

	mod->mod_status = MOD_READY;
	mod_add(mod);

	return mod;

fail:
	if(err == 0)
		plat_mod_close(&mod->mod_library);

	logmsg(LOG_WARN, "Failed to load module %s!", path_name);

	mod_destroy(mod);

	return NULL;
}

/**
 * Report error from module mod
 */
int mod_error(module_t* mod, char* fmtstr, ...) {
	va_list args;

	mod->mod_status = MOD_CONFIG_ERROR;

	va_start(args, fmtstr);
	aas_vprintf(&mod->mod_status_msg, fmtstr, args);
	va_end(args);

	logmsg(LOG_WARN, "Error in module %s: %s", mod->mod_name, mod->mod_status_msg);
	
	return 0;
}

int mod_init(void) {
	mod_search_path = getenv("TS_MODPATH");
	if(mod_search_path == NULL)
		return 1;
	
	mp_cache_init(&mod_cache, module_t);

	load_modules();

	return 0;
}

void mod_fini(void) {
	unload_modules();

	mp_cache_destroy(&mod_cache);
}

