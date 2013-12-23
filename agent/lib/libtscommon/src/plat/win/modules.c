/*
 * modules.c
 *
 *  Created on: 22.12.2012
 *      Author: myaut
 */

#include <defs.h>
#include <plat/win/modules.h>

#include <windows.h>

PLATAPI int plat_mod_open(plat_mod_library_t* lib, const char* path) {
	int error;

	lib->lib_library = LoadLibrary(path);

	if(!lib->lib_library) {
		return GetLastError();
	}

	return 0;
}

PLATAPI int plat_mod_close(plat_mod_library_t* lib) {
	if(FreeLibrary(lib->lib_library) != 0) {
		return GetLastError();
	}

	return 0;
}

PLATAPI void* plat_mod_load_symbol(plat_mod_library_t* lib, const char* name) {
	return GetProcAddress(lib->lib_library, name);
}

PLATAPI char* plat_mod_error_msg(void) {
	/* FIXME: Implement with FormatMessage */
	return "???";
}
