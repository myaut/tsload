
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

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



#ifndef EXECPATH_H_
#define EXECPATH_H_

#include <tsload/defs.h>


/**
 * Returns absolute path to current executable.
 *
 * @return Pointer to static buffer that contains path to current executable or NULL \
 * 		if operation was failed.
 *
 * @note Since it it is universal for entire address space, it \
 *    returns pointer to static buffer.
 * @note Implementation is found here \
 *   [Finding current executable's path without /proc/self/exe](http://stackoverflow.com/questions/1023306/finding-current-executables-path-without-proc-self-exe)
 */
LIBEXPORT PLATAPI const char* plat_execpath(void);

#endif /* EXECPATH_H_ */

