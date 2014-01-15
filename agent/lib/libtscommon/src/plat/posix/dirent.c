/*
 * dirent.c
 *
 *  Created on: 23.12.2012
 *      Author: myaut
 */

#include <defs.h>
#include <pathutil.h>
#include <mempool.h>
#include <string.h>

#include <plat/dirent.h>
#include <tsdirent.h>

#include <unistd.h>
#include <sys/stat.h>

PLATAPI plat_dir_t* plat_opendir(const char *name) {
	plat_dir_t* dir;
	DIR* d_dirp = opendir(name);

	if(d_dirp == NULL)
		return NULL;

	dir = mp_malloc(sizeof(plat_dir_t));

	dir->d_dirp = d_dirp;
	strncpy(dir->d_path, name, PATHMAXLEN);

	return dir;
}

PLATAPI plat_dir_entry_t* plat_readdir(plat_dir_t *dirp) {
	struct dirent* d_entry = readdir(dirp->d_dirp);

	if(d_entry == NULL)
		return NULL;

	dirp->d_entry._d_entry = d_entry;
	dirp->d_entry.d_name   = d_entry->d_name;

	return &dirp->d_entry;
}

PLATAPI int plat_closedir(plat_dir_t *dirp) {
	int ret = closedir(dirp->d_dirp);

	mp_free(dirp);

	return ret;
}

/* DIR in POSIX platforms may not contain d_type field, so we:
 *
 * 1. Form path $DIR/$FILENAME
 * 2. Read file information using stat(2)
 * 3. Process st_mode field from stat structure
 *
 * Also to save $DIR name, we use plat_dir_t structure that proxies
 * requests to readdir/opendir/closedir */
PLATAPI plat_dirent_type_t plat_dirent_type(plat_dir_entry_t* d_entry) {
	struct stat statbuf;
	char path[PATHMAXLEN];

	plat_dir_t* dir = container_of(d_entry, plat_dir_t, d_entry);

	if(d_entry->d_name[0] == '.') {
		if(d_entry->d_name[1] == 0) {
			return DET_CURRENT_DIR;
		}

		if(d_entry->d_name[1] == '.' &&
		   d_entry->d_name[2] == 0) {
			return DET_PARENT_DIR;
		}
	}

	path_join(path, PATHMAXLEN, dir->d_path, d_entry->d_name, NULL);

	stat(path, &statbuf);

	switch(statbuf.st_mode & S_IFMT) {
		case S_IFREG:	return DET_REG;
		case S_IFDIR:	return DET_DIR;
		case S_IFLNK:	return DET_SYMLINK;
		case S_IFBLK:	return DET_BLOCK_DEV;
		case S_IFCHR:	return DET_CHAR_DEV;

		case S_IFIFO:	return DET_PIPE;
		case S_IFSOCK:	return DET_SOCKET;
	}

	return DET_UNKNOWN;
}

PLATAPI boolean_t plat_dirent_hidden(plat_dir_entry_t *d_entry) {
	if(d_entry->d_name[0] == '.') {
		if(d_entry->d_name[1] == 0) {
			return B_FALSE;
		}

		if(d_entry->d_name[1] == '.' &&
		   d_entry->d_name[2] == 0) {
			return B_FALSE;
		}

		return B_TRUE;
	}

	return B_FALSE;
}
