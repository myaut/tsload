/*
 * iofile.c
 *
 *  Created on: 10.02.2013
 *      Author: myaut
 */

#include <defs.h>
#include <simpleio.h>

#include <stdio.h>
#include <plat/iofile.h>

#include <unistd.h>
#include <fcntl.h>

PLATAPI int io_file_open(io_file_t* iof,
						 boolean_t writeable,
						 boolean_t sync) {
	int o_flags = ((iof->iof_exists)? 0 : O_CREAT)	|
				  ((writeable) ? O_WRONLY : O_RDONLY) |
				  ((sync)? O_DSYNC : 0);

	iof->iof_impl.fd = open(iof->iof_path, o_flags, 0660);

	if(iof->iof_impl.fd == -1)
		return -1;

	return 0;
}

PLATAPI int io_file_close(io_file_t* iof,
						  boolean_t do_remove) {
	int ret = close(iof->iof_impl.fd);

	if(do_remove) {
		remove(iof->iof_path);
	}

	return ret;
}

PLATAPI int io_file_seek(io_file_t* iof,
						 uint64_t where) {
	return lseek(iof->iof_impl.fd, where, SEEK_SET);
}

PLATAPI int io_file_read(io_file_t* iof,
						 void* buffer,
						 size_t count) {
	return read(iof->iof_impl.fd, buffer, count);
}

PLATAPI int io_file_write(io_file_t* iof,
						  void* buffer,
						  size_t count) {
	return write(iof->iof_impl.fd, buffer, count);
}
