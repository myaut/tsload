
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



#ifndef TSDIRENT_H_
#define TSDIRENT_H_

#include <tsload/defs.h>

#include <tsload/plat/dirent.h>


/**
 * @module FileSystem directories
 *
 * Platform-independent API for directory listing
 */

typedef enum {
	DET_UNKNOWN = -3,

	DET_PARENT_DIR,
	DET_CURRENT_DIR,

	DET_REG,
	DET_DIR,

	DET_CHAR_DEV,
	DET_BLOCK_DEV,

	DET_SOCKET,
	DET_PIPE,

	DET_SYMLINK,

	DET_DOOR		/* Solaris-specific */
} plat_dirent_type_t;

LIBEXPORT PLATAPI plat_dir_t* plat_opendir(const char *name);
LIBEXPORT PLATAPI plat_dir_entry_t* plat_readdir(plat_dir_t *dirp);
LIBEXPORT PLATAPI int plat_closedir(plat_dir_t *dirp);

LIBEXPORT PLATAPI plat_dirent_type_t plat_dirent_type(plat_dir_entry_t *d_entry);

/**
 * Returns B_TRUE if file referenced by d_entry is considered hidden.
 *
 * On Unix/Linux it is file which name starts from dot
 * On Windows it is determined by file attributes
 */
LIBEXPORT PLATAPI boolean_t plat_dirent_hidden(plat_dir_entry_t *d_entry);

#endif /* DIRENT_H_ */

