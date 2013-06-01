/*
 * dirent.h
 *
 *  Created on: 23.12.2012
 *      Author: myaut
 */

#ifndef PLAT_POSIX_DIRENT_H_
#define PLAT_POSIX_DIRENT_H_

#include <sys/types.h>
#include <dirent.h>

typedef struct {
	struct dirent* _d_entry;

	char* 		  d_name;
} plat_dir_entry_t;

typedef struct {
	plat_dir_entry_t d_entry;

	DIR* d_dirp;

	char 		  d_path[MAXNAMLEN];
} plat_dir_t;

#endif /* PLAT_POSIX_DIRENT_H_ */
