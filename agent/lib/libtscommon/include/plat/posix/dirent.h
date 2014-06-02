
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



#ifndef PLAT_POSIX_DIRENT_H_
#define PLAT_POSIX_DIRENT_H_

#include <pathutil.h>

#include <sys/types.h>
#include <dirent.h>

typedef struct {
	struct dirent* _d_entry;

	char* 		  d_name;
} plat_dir_entry_t;

typedef struct {
	plat_dir_entry_t d_entry;

	DIR* d_dirp;

	char 		  d_path[PATHMAXLEN];
} plat_dir_t;

#endif /* PLAT_POSIX_DIRENT_H_ */

