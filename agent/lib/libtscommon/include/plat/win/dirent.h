
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

