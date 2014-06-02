
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
	if(d_entry->d_name[0] == '.') {
		if(d_entry->d_name[1] == 0) {
			return DET_CURRENT_DIR;
		}

		if(d_entry->d_name[1] == '.' &&
		   d_entry->d_name[2] == 0) {
			return DET_PARENT_DIR;
		}
	}

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


