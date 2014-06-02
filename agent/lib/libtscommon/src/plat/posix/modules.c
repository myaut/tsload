
/*
    This file is part of TSLoad.
    Copyright 2012-2013, Sergey Klyaus, ITMO University

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

