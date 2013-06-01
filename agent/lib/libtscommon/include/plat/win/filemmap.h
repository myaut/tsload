/*
 * filemmap.h
 *
 *  Created on: 09.01.2013
 *      Author: myaut
 */

#ifndef PLAT_WIN_FILEMMAP_H_
#define PLAT_WIN_FILEMMAP_H_

#include <windows.h>

#define MMFL_RDONLY		OF_READ
#define MMFL_WRONLY		OF_WRITE
#define MMFL_RDWR		OF_READWRITE

typedef struct {
	int 	mmf_mode;
	HFILE	mmf_file;
	HANDLE  mmf_map;
} mmap_file_t;


#endif /* PLAT_WIN_FILEMMAP_H_ */
