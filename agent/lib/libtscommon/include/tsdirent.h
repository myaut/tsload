/*
 * tsdirent.h
 *
 *  Created on: 23.12.2012
 *      Author: myaut
 */

#ifndef TSDIRENT_H_
#define TSDIRENT_H_

#include <defs.h>
#include <plat/dirent.h>

/**
 * @module FileSystem directories
 *
 * Platform-independent API for directory listing
 */

typedef enum {
	DET_UNKNOWN = -1,
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
