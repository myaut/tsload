/*
 * version.c
 *
 *  Created on: 15.01.2013
 *      Author: myaut
 */

#include <tsversion.h>
#include <genbuild.h>

#include <stdio.h>

LIBEXPORT void print_ts_version(const char* banner) {
	printf(BUILD_PROJECT " " BUILD_VERSION " [%s]\n", banner);

	puts("");

	puts("Platform: " BUILD_PLATFORM);
	puts("Machine architecture: " BUILD_MACH);

	puts("");

	puts("Build: " BUILD_USER "@" BUILD_HOST " at " BUILD_DATETIME);
	puts("Command line: " BUILD_CMDLINE);
	puts("Compiler: " BUILD_COMPILER);
}
