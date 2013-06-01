/*
 * getopt.c
 *
 *  Created on: 22.12.2012
 *      Author: myaut
 */

#include <defs.h>

#include <unistd.h>

PLATAPI int plat_getopt(int argc, const char* argv[], const char* options) {
	return getopt(argc, argv, options);
}
