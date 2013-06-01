/*
 * getopt.h
 *
 *  Created on: 22.12.2012
 *      Author: myaut
 */

#ifndef GETOPT_H_
#define GETOPT_H_

#include <defs.h>

LIBIMPORT int opterr;
LIBIMPORT int optind;
LIBIMPORT int optopt;
LIBIMPORT char *optarg;

LIBEXPORT PLATAPI int plat_getopt(int argc, const char* argv[], const char* options);

#endif /* GETOPT_H_ */
