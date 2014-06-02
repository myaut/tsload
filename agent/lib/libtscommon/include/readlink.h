
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



#ifndef READLINK_H_
#define READLINK_H_

#include <defs.h>

#include <stdlib.h>

/**
 * Read's symlink (may be determined by DET_SYMLINK) and returns original file path.
 *
 * @param path 		path to symbolic link
 * @param buffer	destination buffer
 * @param buflen    length of destination buffer
 *
 * @note may not be implemented on some platforms
 *
 * @return length of original file path. In case of error returns -1
 */
PLATAPI int plat_readlink(const char* path, char* buffer, size_t buflen);

#endif /* READLINK_H_ */

