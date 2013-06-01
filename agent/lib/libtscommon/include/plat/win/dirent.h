/*
 * dirent.h
 *
 *  Created on: 23.12.2012
 *      Author: myaut
 */

#ifndef PLAT_WIN_DIRENT_H_
#define PLAT_WIN_DIRENT_H_

#include <windows.h>

typedef struct {
	WIN32_FIND_DATA d_find_data;

	char           d_name[256]; /* filename */
} plat_dir_entry_t;

typedef struct {
	HANDLE 	d_handle;

	char 	d_path[MAX_PATH];

	plat_dir_entry_t d_entry;	/* Current Entry */
} plat_dir_t;

#endif /* PLAT_WIN_DIRENT_H_ */
