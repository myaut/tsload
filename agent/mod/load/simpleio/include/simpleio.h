/*
 * dummy.h
 *
 *  Created on: 08.11.2012
 *      Author: myaut
 */

#ifndef DUMMY_H_
#define DUMMY_H_

#include <defs.h>
#include <pathutil.h>

#include <wlparam.h>

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
	wlp_size_t file_size;
	wlp_size_t block_size;

	wlp_string_t path[512];

	wlp_bool_t	 sparse;
	wlp_bool_t 	 sync;

	boolean_t	do_remove;
	io_file_t	iof;

	void* block;
};

#endif /* DUMMY_H_ */
