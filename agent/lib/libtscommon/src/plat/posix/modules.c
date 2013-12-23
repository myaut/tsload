/*
 * modules.c
 *
 *  Created on: 22.12.2012
 *      Author: myaut
 */

#include <defs.h>

#include <plat/posix/modules.h>

#include <dlfcn.h>

PLATAPI int plat_mod_open(plat_mod_library_t* lib, const char* path) {
	int error;

	lib->lib_library = dlopen(path, RTLD_NOW | RTLD_LOCAL);

	if(!lib->lib_library) {
		return 1;
	}

	return 0;
}

PLATAPI int plat_mod_close(plat_mod_library_t* lib) {
	if(dlclose(lib->lib_library) != 0) {
		return 1;
	}

	return 0;
}

PLATAPI void* plat_mod_load_symbol(plat_mod_library_t* lib, const char* name) {
	return dlsym(lib->lib_library, name);
}

PLATAPI char* plat_mod_error_msg(void) {
	return dlerror();
}
