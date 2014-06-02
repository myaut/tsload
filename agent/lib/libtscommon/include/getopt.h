
/*
    This file is part of TSLoad.
    Copyright 2012-2013, Sergey Klyaus, ITMO University

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



#ifndef GETOPT_H_
#define GETOPT_H_

#include <defs.h>

LIBIMPORT int opterr;
LIBIMPORT int optind;
LIBIMPORT int optopt;
LIBIMPORT char *optarg;

LIBEXPORT PLATAPI int plat_getopt(int argc, const char* argv[], const char* options);

#endif /* GETOPT_H_ */

