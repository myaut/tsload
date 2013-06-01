/*
 * filemmap.h
 *
 *  Created on: 09.01.2013
 *      Author: myaut
 */

#ifndef PLAT_POSIX_FILEMMAP_H_
#define PLAT_POSIX_FILEMMAP_H_

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
