
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



#include <tsload/defs.h>

#include <tsload/pathutil.h>
#include <tsload/autostring.h>
#include <tsload/readlink.h>

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
    char* end;
    int i = 0;
    size_t idx = 0;
    size_t part_len = 0;

    dest[0] = '\0';

    while(part != NULL && i < num_parts) {
        part_len = strlen(part);
        if(part_len > len)
            return NULL;

        /* If previous part was not finished by separator, add it*/
        if(i != 0 && !path_is_separator(dest + idx - 1)) {
        	path_append_separator(dest, len);

            len -= PATH_SEP_LENGTH;
            idx += PATH_SEP_LENGTH;
        }

        strncat(dest + idx, part, len);

        idx += part_len;
        len -= part_len;

        part = parts[++i];
    }

#ifdef PLAT_WIN
	/* On Windows trailing slashes are not allowed i.e. in FindFirstFileEx()
	 * so if it is a last part and part is empty, ignore it */
    end = dest + idx - PATH_SEP_LENGTH;
    if(path_is_separator(end))
    	*end = '\0';
#endif

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

        /* Too much parts */
        if(i == PATHMAXPARTS)
            return NULL;
    } while(part != NULL);
    va_end(va);

    return path_join_array(dest, len, i, parts);
}

/**
 * Join paths into auto-allocated string
 *
 * @note Last argument should be always NULL
 *
 * @see path_join
 * @see aas_init
 * @see aas_free
 */
char* path_join_aas(char** aas, ...) {
	const char* parts[PATHMAXPARTS];
	const char* part = NULL;

	va_list va;

	int i = 0;
	size_t count = 0;

	va_start(va, aas);
	do {
		part = va_arg(va, const char*);
		parts[i++] = part;

		if(part) {
			count += strlen(part) + PATH_SEP_LENGTH;
		}

		/* Too much parts */
		if(i == PATHMAXPARTS)
			return NULL;
	} while(part != NULL);
	va_end(va);

	*aas = aas_allocate(count);
	if(*aas == NULL)
		return NULL;

	return path_join_array(*aas, count + 1, i, parts);
}

static int path_split_add(path_split_iter_t* iter, const char* part, const char* end) {
    size_t len = end - part;

    if(len == 0 || (path_cmp_n(part, PATH_CURDIR, len) == 0)) {
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

static const char* str_find(const char* s, 
							boolean_t (*pred)(const char* s)) {
    while(!(pred(s))) {
        if(*s == '\0')
            return NULL;

        ++s;
    }

    return s;
}


static const char* str_find_reverse(const char* s, const char* from, 
									boolean_t (*pred)(const char* s)) {
    do {
        if(from == s)
            return NULL;

        --from;
    } while(!(pred(from)));

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
const char* path_split(path_split_iter_t* iter, int max, const char* path) {
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
    if(max > 0 && path_is_separator(path)) {
    	iter->ps_parts[0] = "";
    	iter->ps_num_parts = 1;
    	iter->ps_last_is_root = B_FALSE;
    }
    else {
    	iter->ps_parts[0] = NULL;
		iter->ps_num_parts = 0;
		iter->ps_last_is_root = path_is_abs(path);
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

        while((begin = str_find_reverse(path, begin, path_is_separator)) != NULL) {
            if(max == -1)
                break;

            max += path_split_add(iter, begin + PATH_SEP_LENGTH, orig);

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

        while((end = str_find(end, path_is_separator)) != NULL) {
            if(max == 1)
                break;

            max -= path_split_add(iter, orig, end);

            orig = end + PATH_SEP_LENGTH;
            end += PATH_SEP_LENGTH;
        }

        path_split_add(iter, orig, path + strlen(path));
    }

    return iter->ps_parts[0];
}

/**
 * Returns next path part from iterator or NULL if all parts was
 * walked.
 * */
const char* path_split_next(path_split_iter_t* iter) {
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
const char* path_split_reset(path_split_iter_t* iter) {
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
	const char* part_abspath;
	const char* part_path;

	/* If path is empty - simply return abspath */
	if(strlen(path) == 0 ||
	   path_cmp(path, PATH_CURDIR) == 0) {
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
		if(path_cmp(part_path, part_abspath) != 0) {
			return NULL;
		}

		part_path = path_split_next(&si_path);
		part_abspath = path_split_next(&si_abspath);
	}

	strncpy(result, part_abspath, len);
	return result;
}

/**
 * Based on argument `arg` provided by user deduces if he provided 
 * path to configuration file (which name is defined by `cfgfname`)
 * or to a directory containing it. 
 * 
 * @note This function doesn't check if file exists, directory is accessible, etc.
 * 
 * @param cfgdir resulting buffer containing directory with config
 * @param cfglen length of cfgdir buffer
 * @param cfgfname default file name for configuration file
 * @param arg argument entered by user
 * 
 * @return `argdir`
 */
char* path_argfile(char* cfgdir, size_t cfglen, const char* cfgfname, const char* arg) {
	size_t arglen = strlen(arg);
	
	const char* tail;
	path_split_iter_t iter;
	
	/* If arg matches to file name, or is current directory, copy current dir to cfgdir. */
	if(path_cmp(arg, cfgfname) == 0 || path_cmp(PATH_CURDIR, arg) == 0) {
		strncpy(cfgdir, PATH_CURDIR, cfglen);
		return cfgdir;
	}
	
	/* If arg ends with path separator, than it is obviously a directory. Separator is removed. */
	tail = arg + arglen - PATH_SEP_LENGTH;
	if(path_is_separator(tail)) {
		strncpy(cfgdir, arg, cfglen);		
		*(cfgdir + strlen(cfgdir) - PATH_SEP_LENGTH) = '\0';
		
		return cfgdir;
	}	
	
	/* Now check if last element is cfgfname */
	tail = path_basename(&iter, arg);
	if(tail == NULL || path_cmp(tail, cfgfname) != 0) {
		strncpy(cfgdir, arg, cfglen);
		return cfgdir;
	}
	
	/* Re-use what left from path_basename here */
	strncpy(cfgdir, path_split_next(&iter), cfglen);
	return cfgdir;
}

/**
 * Read symbolic link and if its destination is not absolute, 
 * make it relative to path. If path was absolute, will make destination
 * path absolute too.
 * 
 * @param linkpath resulting buffer containing link destination
 * @param linklen length of `linkpath`
 * @param path path to symbolic link
 * 
 * @see path_join_array
 * 
 * @return NULL if readlink()/path_split() was unsuccessful or `path_join_array` result
 */
char* path_abslink(char* linkpath, size_t linklen, const char* path) {
	const char* parts[PATHMAXPARTS];
	char tmppath[PATHMAXLEN];
	const char* part0;
	
	int tmpidx, pathidx;
	path_split_iter_t tmpiter, pathiter;
	
	if(plat_readlink(path, tmppath, PATHMAXLEN) == -1)
		return NULL;
	
	if(path_split(&tmpiter, PATHMAXPARTS, tmppath) == NULL)
		return NULL;
	if(path_split(&pathiter, PATHMAXPARTS, path) == NULL)
		return NULL;
	
	/* If path is absolute, ignore building path and return it as is */
	/* FIXME: check for absolute path in Windows */
	part0 = tmpiter.ps_parts[0];
	if(part0 && *part0 == '\0') {
		strncpy(linkpath, tmppath, linklen);
		return linkpath;
	}
	
	/* Copy all except last parts from path to linkpath */
	for(pathidx = 0; pathidx < pathiter.ps_num_parts - 1; ++pathidx) {
		parts[pathidx] = pathiter.ps_parts[pathidx];
	}
	
	for(tmpidx = 0; tmpidx < tmpiter.ps_num_parts; ++tmpidx) {
		part0 = tmpiter.ps_parts[tmpidx];
		
		if(strcmp(part0, PATH_CURDIR) == 0) {
			continue;
		}
		if(strcmp(part0, PATH_PARENTDIR) == 0) {
			pathidx = max(pathidx - 1, 0);
			continue;
		}
		
		parts[pathidx++] = part0;
	}
	
	return path_join_array(linkpath, linklen, pathidx, parts);
}