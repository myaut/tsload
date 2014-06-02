
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

