
/*
    This file is part of TSLoad.
    Copyright 2015, Sergey Klyaus, Tune-IT

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

#ifndef PLAT_WIN_PATHUTIL_H_
#define PLAT_WIN_PATHUTIL_H_

#include <string.h>

#define PATH_CURDIR				"."
#define PATH_PARENTDIR			".."
#define PATH_SEP_LENGTH			1

STATIC_INLINE int path_cmp(const char* a1, const char* a2) {
	return _stricmp(a1, a2);
}
STATIC_INLINE int path_cmp_n(const char* a1, const char* a2, size_t n) {
	return _strnicmp(a1, a2, n);
}

STATIC_INLINE boolean_t path_is_abs(const char* path) {
	/* Check if it is UNC path or it resides on drive */
	return (path[0] == '\\' && path[1] == '\\') || path[1] == ':';
}

STATIC_INLINE boolean_t path_is_separator(const char* path) {
	/* Windows can support both separators, so... */
	return *path == '/' || *path == '\\';
}

STATIC_INLINE void path_append_separator(char* path, size_t len) {
	/* In Windows we want to keep uniform separators across path */
	if(strchr(path, "/") != NULL)
		strncat(path, "/", len);
	else
		strncat(path, "\\", len);
}

#endif