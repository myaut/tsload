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

PLATAPI int plat_readlink(const char* path, char* buffer, size_t buflen);

#endif /* READLINK_H_ */
