/*
 * execpath.h
 *
 *  Created on: Jan 13, 2014
 *      Author: myaut
 */

#ifndef EXECPATH_H_
#define EXECPATH_H_

#include <defs.h>

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
