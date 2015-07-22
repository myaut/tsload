
/*
    This file is part of TSLoad.
    Copyright 2013, Sergey Klyaus, ITMO University

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



#include <tsload/defs.h>

#include <tsload/version.h>
#include <tsload/genbuild.h>

#include <stdio.h>


LIBEXPORT void print_ts_version(const char* banner) {
	printf(BUILD_PROJECT " " BUILD_VERSION " [%s]\n", banner);
	puts("Copyright (C) " BUILD_YEAR " " BUILD_AUTHOR);

	puts("");

	puts("Platform: " BUILD_PLATFORM);
	puts("Machine architecture: " BUILD_MACH);

	puts("");

	puts("Build: " BUILD_USER "@" BUILD_HOST " at " BUILD_DATETIME);
	puts("Command line: " BUILD_CMDLINE);
	puts("Compiler: " BUILD_COMPILER);
}

