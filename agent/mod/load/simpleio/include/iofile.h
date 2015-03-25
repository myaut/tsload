
/*
    This file is part of TSLoad.
    Copyright 2012-2015, Sergey Klyaus, Tune-IT

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



#ifndef MOD_LOAD_IOFILE_H_
#define MOD_LOAD_IOFILE_H_

#include <tsload/pathutil.h>

#include <mod/simpleio/plat/iofile.h>

#include <stdlib.h>

typedef enum { IOF_REGULAR, IOF_BLOCKDEV } io_file_type_t;

typedef struct {
	plat_io_file_t	iof_impl;

	AUTOSTRING char* iof_path;

	boolean_t		iof_exists;
	io_file_type_t 	iof_file_type;
	uint64_t		iof_file_size;
	
	AUTOSTRING char* iof_error_msg;
} io_file_t;

/* I/O file interface provides cross-platform access
 * to both regular files and raw disks */
PLATAPI int io_file_init(io_file_t* iof, io_file_type_t type, const char* path, uint64_t file_size);
PLATAPI int io_file_stat(io_file_t* iof);

PLATAPI int io_file_open(io_file_t* iof, boolean_t rdwr, boolean_t sync);
PLATAPI void io_file_close(io_file_t* iof, boolean_t do_remove);

PLATAPI int io_file_fsync(io_file_t* iof);

PLATAPI int io_file_seek(io_file_t* iof, uint64_t where);
PLATAPI int io_file_seek_sparse(io_file_t* iof, uint64_t where);

PLATAPI int io_file_pread(io_file_t* iof, void* buffer, size_t count, uint64_t offset);
PLATAPI int io_file_pwrite(io_file_t* iof, void* buffer, size_t count, uint64_t offset);

#endif