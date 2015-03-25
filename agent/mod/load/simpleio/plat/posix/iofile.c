
/*
    This file is part of TSLoad.
    Copyright 2013-2015, Sergey Klyaus, ITMO University

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



#include <tsload/defs.h>

#include <simpleio.h>
#include <mod/simpleio/plat/iofile.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

PLATAPI int io_file_init(io_file_t* iof, io_file_type_t type, const char* path, uint64_t file_size) {
	iof->iof_impl.fd = -1;
	
	aas_copy(aas_init(&iof->iof_path), path);
	
	iof->iof_exists = B_FALSE;
	iof->iof_file_size = file_size;
	iof->iof_file_type = type;
	
	aas_init(&iof->iof_error_msg);
	
	return 0;
}

PLATAPI int io_file_stat(io_file_t* iof) {
	struct stat s;

	iof->iof_exists = stat(iof->iof_path, &s) != -1;
	
	return 0;
}

PLATAPI int io_file_open(io_file_t* iof, boolean_t rdwr, boolean_t sync) {
	int o_flags = ((iof->iof_exists)? 0 : O_CREAT)	|
				  ((rdwr) ? O_RDWR : O_RDONLY) |
				  ((sync)? O_DSYNC : 0);
	int fd;
	
	fd = open(iof->iof_path, o_flags, 0660);

	if(fd == -1 && iof->iof_error_msg == NULL) {
		aas_printf(&iof->iof_error_msg, "Failed to open('%s', 0x%x): %s", 
				   iof->iof_path, o_flags, strerror(errno));
		return -1;
	}
	
	iof->iof_impl.fd = fd;

	return 0;
}

PLATAPI void io_file_close(io_file_t* iof, boolean_t do_remove) {
	if(iof->iof_impl.fd != -1) {
		close(iof->iof_impl.fd);

		if(do_remove) {
			remove(iof->iof_path);
		}
		
		iof->iof_impl.fd = -1;
	}
	
	aas_free(&iof->iof_path);
	aas_free(&iof->iof_error_msg);
}

PLATAPI int io_file_fsync(io_file_t* iof) {
	int ret = fsync(iof->iof_impl.fd);
	
	if(ret != 0 && iof->iof_error_msg == NULL) {
		aas_printf(&iof->iof_error_msg, "Failed to fsync('%s'): %s", 
				   iof->iof_path, strerror(errno));
	}
	
	return ret;
}

PLATAPI int io_file_seek(io_file_t* iof, uint64_t where) {
	int ret = lseek(iof->iof_impl.fd, where, SEEK_SET);
	
	if(ret != 0 && iof->iof_error_msg == NULL) {
		aas_printf(&iof->iof_error_msg, "Failed to lseek('%s', %" PRId64 "): %s", 
				   iof->iof_path, where, strerror(errno));
	}
	
	return ret;
}

PLATAPI int io_file_pread(io_file_t* iof, void* buffer, size_t count, uint64_t offset) {
	return pread(iof->iof_impl.fd, buffer, count, offset);
}

PLATAPI int io_file_pwrite(io_file_t* iof, void* buffer, size_t count, uint64_t offset) {
	return pwrite(iof->iof_impl.fd, buffer, count, offset);
}

