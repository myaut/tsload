
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



#include <tsload/defs.h>

#include <tsload/pathutil.h>

#include <simpleio.h>

#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <fcntl.h>


int iofile_get_size(io_file_t* iof, struct stat* s) {
	if((s->st_mode & S_IFMT) == S_IFREG) {
		iof->iof_file_type = IOF_REGULAR;
		iof->iof_file_size = 512 * s->st_blocks;

		return 0;
	}
	else if((s->st_mode & S_IFMT) == S_IFBLK) {
		int fd;

		iof->iof_file_type = IOF_BLOCKDEV;

		fd = open(iof->iof_path, O_RDONLY);
		ioctl(fd, BLKGETSIZE64, &iof->iof_file_size);
		close(fd);

		return 0;
	}

	return -1;
}

PLATAPI int io_file_stat(io_file_t* iof,
						 const char* path) {
	struct stat s;

	strncpy(iof->iof_path, path, PATHMAXLEN);

	if(stat(iof->iof_path, &s) == -1) {
		iof->iof_exists = B_FALSE;
		return 0;
	}

	iof->iof_exists = B_TRUE;

	if(iofile_get_size(iof, &s) == -1)
		return -1;

	return 0;
}

