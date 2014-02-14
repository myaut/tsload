/*
 * filelock.c
 *
 *  Created on: Feb 12, 2014
 *      Author: myaut
 */

#include <filelock.h>

#include <sys/file.h>

PLATAPI int plat_flock(int fd, int operation) {
	return flock(fd, operation);
}
