/*
 * dirent.c
 *
 *  Created on: 30.12.2012
 *      Author: myaut
 */

#include <defs.h>

#include <tsdirent.h>

#include <dirent.h>

PLATAPI plat_dir_t* plat_opendir(const char *name) {
	return opendir(name);
}

PLATAPI plat_dir_entry_t* plat_readdir(plat_dir_t *dirp) {
	return readdir(dirp);
}

PLATAPI int plat_closedir(plat_dir_t *dirp) {
	return closedir(dirp);
}

PLATAPI plat_dirent_type_t plat_dirent_type(plat_dir_entry_t* d_entry) {
	switch(d_entry->d_type) {
		case DT_REG:	return DET_REG;
		case DT_DIR:	return DET_DIR;
		case DT_LNK:	return DET_SYMLINK;
		case DT_BLK:	return DET_BLOCK_DEV;
		case DT_CHR:	return DET_CHAR_DEV;

		case DT_FIFO:	return DET_PIPE;
		case DT_SOCK:	return DET_SOCKET;
	}

	return DET_UNKNOWN;
}

