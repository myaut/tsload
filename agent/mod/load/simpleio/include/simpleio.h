
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



#ifndef DUMMY_H_
#define DUMMY_H_

#include <tsload/defs.h>

#include <tsload/pathutil.h>

#include <tsload/load/wlparam.h>

#include <plat/iofile.h>


enum simpleio_test {
	SIMPLEIO_READ,
	SIMPLEIO_WRITE
};

typedef enum { IOF_REGULAR, IOF_BLOCKDEV } io_file_type_t;

typedef struct {
	plat_io_file_t	iof_impl;

	char 			iof_path[PATHMAXLEN];

	boolean_t		iof_exists;
	io_file_type_t 	iof_file_type;
	uint64_t		iof_file_size;
} io_file_t;

/* I/O file interface provides cross-platform access
 * to both regular files and raw disks */
PLATAPI int io_file_stat(io_file_t* iof,
						   const char* path);

PLATAPI int io_file_open(io_file_t* iof,
						 boolean_t writeable,
						 boolean_t sync);

PLATAPI int io_file_close(io_file_t* iof,
						  boolean_t do_remove);

PLATAPI int io_file_seek(io_file_t* iof,
						 uint64_t where);

PLATAPI int io_file_read(io_file_t* iof,
						 void* buffer,
						 size_t count);

PLATAPI int io_file_write(io_file_t* iof,
						  void* buffer,
						  size_t count);

struct simpleio_workload {
	wlp_integer_t file_size;
	wlp_integer_t block_size;

	wlp_string_t path[512];

	wlp_bool_t	 sparse;
	wlp_bool_t 	 sync;

	boolean_t	do_remove;
	io_file_t	iof;

	void* block;
};

#endif /* DUMMY_H_ */

