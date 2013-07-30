/*
 * readlink.c
 *
 *  Created on: 30.07.2013
 *      Author: myaut
 */

#include <readlink.h>
#include <unistd.h>

PLATAPI int plat_readlink(const char* path, char* buffer, size_t buflen) {
	size_t len = readlink(path, buffer, buflen);

	if((int) len != -1) {
		buffer[len] = '\0';
	}

	return len;
}
