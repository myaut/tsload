
/*
    This file is part of TSLoad.
    Copyright 2012-2014, Sergey Klyaus, ITMO University

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



#ifndef FNUTIL_H_
#define FNUTIL_H_

#include <tsload/defs.h>

#include <stdlib.h>


/**
 * @module Path processing utilities
 */

/**
 * Maximum length of path
 */
#define PATHMAXLEN      1024
#define PATHPARTMAXLEN	256

#define PATHMAXPARTS   32

/**
 * Temporary storage for path splitting operations
 */
typedef struct {
    char ps_storage[PATHMAXLEN];
    char* ps_dest;

    int     ps_part;
    int     ps_num_parts;
    char*  ps_parts[PATHMAXPARTS];

#ifdef PLAT_POSIX
    boolean_t ps_last_is_root;
#endif
} path_split_iter_t;

LIBEXPORT char* path_join_array(char* dest, size_t len, int num_parts, const char** parts);
LIBEXPORT char* path_join(char* dest, size_t len, ...);
LIBEXPORT char* path_join_aas(char** aas, ...);

LIBEXPORT char* path_split(path_split_iter_t* iter, int max, const char* path);
LIBEXPORT char* path_split_next(path_split_iter_t* iter);
LIBEXPORT char* path_split_reset(path_split_iter_t* iter);

/**
 * Returns directory name. Uses iter as temporary storage
 *
 * @note uses path_split */
STATIC_INLINE char* path_dirname(path_split_iter_t* iter, const char* path) {
	path_split(iter, -2, path);
	return path_split_next(iter);
}

/**
 * Returns name of file. Uses iter as temporary storage
 *
 * @note uses path_split */
STATIC_INLINE char* path_basename(path_split_iter_t* iter, const char* path) {
	return path_split(iter, -2, path);
}

LIBEXPORT char* path_remove(char* result, size_t len, const char* abspath, const char* path);

#endif /* FNUTIL_H_ */
