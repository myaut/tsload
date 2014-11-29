
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



#ifndef PLAT_POSIX_FILEMMAP_H_
#define PLAT_POSIX_FILEMMAP_H_

#include <tsload/defs.h>

#include <stdlib.h>

#include <fcntl.h>


#define MMFL_RDONLY		O_RDONLY
#define MMFL_WRONLY		O_WRONLY
#define MMFL_RDWR		O_RDWR

typedef struct {
	int mmf_fd;
	int mmf_mode;

	size_t mmf_length;
} mmap_file_t;

#endif /* PLAT_WIN_FILEMMAP_H_ */

