/*
 * filemmap.h
 *
 *  Created on: 09.01.2013
 *      Author: myaut
 */

#ifndef FILEMMAP_H_
#define FILEMMAP_H_

#include <defs.h>
#include <plat/filemmap.h>

#define MME_OK				0
#define MME_INVALID_FLAG	-1
#define MME_OPEN_ERROR		-2

#define MMF_MAP_ALL		-1

PLATAPI int mmf_open(mmap_file_t* mmf, const char* filename, int mmfl);
PLATAPI void mmf_close(mmap_file_t* mmf);

PLATAPI void* mmf_create(mmap_file_t* mmf, long long offset, size_t length);
PLATAPI void mmf_destroy(mmap_file_t* mmf, void* mapping);

#endif /* FILEMMAP_H_ */
