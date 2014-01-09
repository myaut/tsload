/*
 * readlink.h
 *
 *  Created on: 30.07.2013
 *      Author: myaut
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
