/*
 * blkdev.c
 *
 *  Created on: 10.02.2013
 *      Author: myaut
 */

#include <defs.h>
#include <pathutil.h>

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
