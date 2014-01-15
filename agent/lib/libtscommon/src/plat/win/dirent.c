/*
 * dirent.c
 *
 *  Created on: 23.12.2012
 *      Author: myaut
 */

#include <defs.h>
#include <mempool.h>

#include <tsdirent.h>

#include <assert.h>

/* Simulate readdir/opendir/closedir behaviour
 * using Windows API */

PLATAPI plat_dir_t* plat_opendir(const char *name) {
	plat_dir_t* dirp = (plat_dir_t*) mp_malloc(sizeof(plat_dir_t));

	dirp->d_handle = NULL;

	strncpy(dirp->d_path, name, MAX_PATH);
	/* Should find all files in directory */
	strncat(dirp->d_path, "\\*", MAX_PATH);

	return dirp;
}

PLATAPI plat_dir_entry_t* plat_readdir(plat_dir_t *dirp) {
	plat_dir_entry_t* d_entry = &dirp->d_entry;

	/* Unlike readdir, Windows API have two functions:
	 * - FindFirstFile that starts searching over directory
	 * - FindNextFile,
	 * so we use them both */
	if(dirp->d_handle) {
		if(!FindNextFile(dirp->d_handle, &d_entry->d_find_data))
			return NULL;
	}
	else {
		dirp->d_handle = FindFirstFile(dirp->d_path,
				                       &d_entry->d_find_data);

		if(ERROR_INVALID_HANDLE == dirp->d_handle)
			return NULL;
	}

	/* Convert WIN32_FIND_DATA to dirent */
	strncpy(d_entry->d_name, d_entry->d_find_data.cFileName, 256);

	return d_entry;
}

PLATAPI int plat_closedir(plat_dir_t *dirp) {
	FindClose(dirp->d_handle);

	mp_free(dirp);

	return 0;
}

PLATAPI plat_dirent_type_t plat_dirent_type(plat_dir_entry_t* d_entry) {
	if(d_entry->d_name[0] == '.') {
		if(d_entry->d_name[1] == 0) {
			return DET_CURRENT_DIR;
		}

		if(d_entry->d_name[1] == '.' &&
		   d_entry->d_name[2] == 0) {
			return DET_PARENT_DIR;
		}
	}

	if(d_entry->d_find_data.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) {
		return DET_CHAR_DEV;
	}
	else if(d_entry->d_find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		return DET_DIR;
	}

	return DET_REG;
}

PLATAPI boolean_t plat_dirent_hidden(plat_dir_entry_t* d_entry) {
	return d_entry->d_find_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN;
}
