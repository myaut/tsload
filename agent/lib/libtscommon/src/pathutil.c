
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



#include <pathutil.h>

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(PLAT_WIN)
const char* path_curdir = ".";
const char* path_separator = "\\";
const int psep_length = 1;

#elif defined(PLAT_POSIX)
const char* path_curdir = ".";
const char* path_separator = "/";
const int psep_length = 1;

#else
#error "Unknown path separator for this platform"

#endif

/**
 * Create path from array into string. Similiar to python's os.path.join
 *
 * @param dest Destination string
 * @param len Length of destination string
 * @param num_parts Number of parts provided
 * @param parts Parts of path
 *
 * @return destination with formed path or NULL if destination was overflown
 *  */
char* path_join_array(char* dest, size_t len, int num_parts, const char** parts) {
    const char* part = parts[0];
    int i = 0;
    int idx = 0;
    size_t part_len = 0;

    dest[0] = '\0';

    while(part != NULL && i < num_parts) {
        part_len = strlen(part);
        if(part_len > len)
            return NULL;

        len -= part_len;

        /* If previous part was not finished by separator,
         * add it (works only if psep_length == 1 */
        if(i != 0 && dest[idx - 1] != *path_separator) {
            len -= psep_length;
            idx += psep_length;
            strncat(dest, path_separator, len);
        }

        strncat(dest, part, len);
        idx += part_len;

        part = parts[++i];
    }

    return dest;
}

/**
 * Create path from various arguments
 * @note Last argument should be always NULL
 *
 * Example: path_join(path, PATHMAXLEN, "/tmp", tmpdir, "file", NULL)
 *
 * @see path_join_array
 */
char* path_join(char* dest, size_t len, ...) {
    const char* parts[PATHMAXPARTS];
    const char* part = NULL;
    va_list va;
    int i = 0;

    va_start(va, len);
    do {
        part = va_arg(va, const char*);
        parts[i++] = part;

        /*Too much parts*/
        if(i == PATHMAXPARTS)
            return NULL;
    } while(part != NULL);
    va_end(va);

    return path_join_array(dest, len, i, parts);
}

static int path_split_add(path_split_iter_t* iter, const char* part, const char* end) {
    size_t len = end - part;

    if(len == 0 || (strncmp(part, path_curdir, len) == 0)) {
    	/* Ignore consecutive separators or references to current path */
		*iter->ps_dest++ = 0;
		return 0;
    }

    /* FIXME: Additional checks */
    strncpy(iter->ps_dest, part, len);

    iter->ps_parts[iter->ps_num_parts++] = iter->ps_dest;
    iter->ps_parts[iter->ps_num_parts] = NULL;

    iter->ps_dest += len;
    *iter->ps_dest++ = 0;

    return 1;
}

static const char* strrchr_x(const char* s, const char* from, int c) {
    do {
        if(from == s)
            return NULL;

        --from;
    } while(*from != c);

    return from;
}

/**
 * Split path into parts. Saves result into path iterator which may be walked using
 * path_split_next. Returns first part of path or NULL if max is too large.
 *
 * If max value is negative, path_split would put paths in reverse order, so
 * first part would be filename.
 *
 * Uses iter as temporary storage, it is re-enterable.
 *
 * First (or last for reverse order) component is root path item. For Windows it is
 * drive letter, on POSIX - empty string.
 *
 * @param iter pre-allocated split path iterator
 * @param max maximum number parts which will be processed
 *
 * @note if number of parts exceeds PATHMAXPARTS, will fail and returns NULL
 *
 * @return Pointer to first part or NULL in case of error
 * */
char* path_split(path_split_iter_t* iter, int max, const char* path) {
    char* dest = iter->ps_storage;
    long max_parts = abs(max);
    size_t part_len = 0;

    /* Invalid max value*/
    if(max_parts == 0 || max_parts > PATHMAXPARTS)
        return NULL;

    /* In Windows we always have 'root component' of path - it's drive letter
     * In POSIX we have to simulate it (for absolute paths only).
     * I.e. for /abc/ it is "", "abc" in straight order and "abc", "" in reverse order */
#if defined(PLAT_POSIX)
    if(max > 0 && *path == *path_separator) {
    	iter->ps_parts[0] = "";
    	iter->ps_num_parts = 1;
    	iter->ps_last_is_root = B_FALSE;
    }
    else {
    	iter->ps_parts[0] = NULL;
		iter->ps_num_parts = 0;
		iter->ps_last_is_root = (*path == *path_separator);
    }
#elif defined(PLAT_WIN)
    iter->ps_parts[0] = NULL;
	iter->ps_num_parts = 0;
#endif

	iter->ps_part = 1;
    iter->ps_dest = iter->ps_storage;

    /* for psep_length != 1 should use strstr or reverse analog */
    if(max < 0) {
        /* Reverse - search */
    	const char *begin, *orig;
    	begin = orig = path + strlen(path);

        while((begin = strrchr_x(path, begin, *path_separator)) != NULL) {
            if(max == -1)
                break;

            max += path_split_add(iter, begin + psep_length, orig);

            orig = begin;
        }

        if(orig != path) {
            /* Relative path - add last part to it:
             * NOTE: On Windows all paths are relative because of C: */
            path_split_add(iter, path, orig);
        }
        else {
        	/* Add first "" */
        	path_split_add(iter, path, path);
        }
    }
    else {
    	const char *end, *orig;
    	end = orig = path;

        while((end = strchr(end, *path_separator)) != NULL) {
            if(max == 1)
                break;

            max -= path_split_add(iter, orig, end);

            orig = end + psep_length;
            end += psep_length;
        }

        path_split_add(iter, orig, path + strlen(path));
    }

    return iter->ps_parts[0];
}

/**
 * Returns next path part from iterator or NULL if all parts was
 * walked.
 * */
char* path_split_next(path_split_iter_t* iter) {
    if(iter->ps_part == iter->ps_num_parts) {
#ifdef PLAT_POSIX
    	if(iter->ps_last_is_root) {
    		iter->ps_last_is_root = B_FALSE;
    		return "";
    	}
#endif

        return NULL;
    }

    return iter->ps_parts[iter->ps_part++];
}

/**
 * Resets iterator to beginning
 * */
char* path_split_reset(path_split_iter_t* iter) {
	iter->ps_part = 1;

	return iter->ps_parts[0];
}

/**
 * Remove relative path element _path_ from the end of absolute path.
 *
 * For example:
 * `path_remove("/opt/tsload/var/tsload", "var/tsload") -> "/opt/tsload"`
 *
 * @param result resulting buffer that contains all elements of abspath \
 * 			but path
 * @param len length of result buffer
 * @param abspath absolute path to be filtered
 * @param path path element
 *
 * @return NULL if path is not element of abspath or result
 */
char* path_remove(char* result, size_t len, const char* abspath, const char* path) {
	path_split_iter_t si_abspath, si_path;
	int count = 1;
	char* part_abspath;
	char* part_path;
	char* root;

	/* If path is empty - simply return abspath */
	if(strlen(path) == 0 ||
	   strcmp(path, path_curdir) == 0) {
			strncpy(result, abspath, len);
			return result;
	}

	/* 1st pass - count path parts in path */
	part_path = path_split(&si_path, -PATHMAXPARTS, path);
	while(part_path != NULL) {
		part_path = path_split_next(&si_path);
		++count;
	}

	/* 2nd pass - split cur_execpath reversely and deduce base dir */
	part_abspath = path_split(&si_abspath, -count, abspath);
	part_path = path_split_reset(&si_path);

	while(--count > 0) {
		if(strcmp(part_path, part_abspath) != 0) {
			return NULL;
		}

		part_path = path_split_next(&si_path);
		part_abspath = path_split_next(&si_abspath);
	}

	strncpy(result, part_abspath, len);
	return result;
}


