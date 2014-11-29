
/*
    This file is part of TSLoad.
    Copyright 2013, Sergey Klyaus, ITMO University

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



#ifndef PLAT_WIN_FILEMMAP_H_
#define PLAT_WIN_FILEMMAP_H_

#include <tsload/defs.h>

#include <windows.h>


#define MMFL_RDONLY		OF_READ
#define MMFL_WRONLY		OF_WRITE
#define MMFL_RDWR		OF_READWRITE

typedef struct {
	int 	mmf_mode;
	HFILE	mmf_file;
	HANDLE  mmf_map;
} mmap_file_t;


#endif /* PLAT_WIN_FILEMMAP_H_ */

