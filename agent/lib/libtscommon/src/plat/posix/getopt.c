/*
 * getopt.c
 *
 *  Created on: 22.12.2012
 *      Author: myaut
 */

#include <defs.h>

extern int getopt(int argc, const char* argv[], const char* options);

PLATAPI int plat_getopt(int argc, const char* argv[], const char* options) {
	return getopt(argc, argv, options);
}
