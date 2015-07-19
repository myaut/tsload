
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

#ifndef PLAT_POSIX_PATHUTIL_H_
#define PLAT_POSIX_PATHUTIL_H_

#include <string.h>

STATIC_INLINE int path_cmp(const char* a1, const char* a2) {
	return strcmp(a1, a2);
}
STATIC_INLINE int path_cmp_n(const char* a1, const char* a2, size_t n) {
	return strncmp(a1, a2, n);
}

STATIC_INLINE boolean_t path_is_abs(const char* path) {
	return path[0] == '/';
}

STATIC_INLINE boolean_t path_is_separator(const char* path) {
	return *path == '/';
}

STATIC_INLINE void path_append_separator(char* path, size_t len) {
	strncat(path, "/", len);
}

#endif