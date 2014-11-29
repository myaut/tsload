
/*
    This file is part of TSLoad.
    Copyright 2013-2014, Sergey Klyaus, ITMO University

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



#ifndef FILEMMAP_H_
#define FILEMMAP_H_

#include <tsload/defs.h>

#include <tsload/plat/filemmap.h>


/**
 * @module Memory-mapped files
 */

/**
 * @name Error codes
 *
 * @value MME_OK - everything went fine
 * @value MME_INVALID_FLAG - invalid mmfl value
 * @value MME_OPEN_ERROR - failed to open file
 * @value MME_MMAP_ERROR - failed to create memory mapping
 * */
#define MME_OK				0
#define MME_INVALID_FLAG	-1
#define MME_OPEN_ERROR		-2
#define MME_MMAP_ERROR		-3

/**
 * Offset value to map entire file into memory
 * */
#define MMF_MAP_ALL		-1

/**
 * @name Memory mapping protection flags
 *
 * @value MMFL_RDONLY map read only
 * @value MMFL_WRONLY map write only
 * @value MMFL_RDWR allows both write and reads
 * */
#ifdef TSDOC
#define MMFL_RDONLY
#define MMFL_WRONLY
#define MMFL_RDWR
#endif

/**
 * Initialize mmap_file_t structure and open file
 *
 * @param mmf pointer to mmap_file_t
 * @param filename path to file
 * @param mmfl memory mapping flag
 *
 * @return MME_OK if file was successfully opened or MME_OPEN_ERROR
 * */
LIBEXPORT PLATAPI int mmf_open(mmap_file_t* mmf, const char* filename, int mmfl);

/**
 * Close memory-mapped file
 *
 * @param mmf pointer to mmap_file_t
 * */
LIBEXPORT PLATAPI void mmf_close(mmap_file_t* mmf);

/**
 * Create new mapping [offset;offset+length] from file
 *
 * @param mmf pointer to mmap_file_t
 * @param offset offset inside file
 * @param length length of mapping area
 * @param mapping_ptr pointer to pointer to mapping area
 *
 * @return MME_OK if mapping was successfully mapped, or MME_MMAP_ERROR/MME_INVALID_FLAG
 * */
LIBEXPORT PLATAPI int mmf_create(mmap_file_t* mmf, long long offset, size_t length, void** mapping_ptr);

/**
 * Unmap file area
 *
 * @param mmf pointer to mmap_file_t
 * @param mapping pointer to mapping returned earliear by mmf_create
 */
LIBEXPORT PLATAPI void mmf_destroy(mmap_file_t* mmf, void* mapping);

#endif /* FILEMMAP_H_ */

