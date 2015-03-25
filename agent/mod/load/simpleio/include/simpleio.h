
/*
    This file is part of TSLoad.
    Copyright 2012-2015, Sergey Klyaus, Tune-IT

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



#ifndef MOD_LOAD_SIMPLEIO_H_
#define MOD_LOAD_SIMPLEIO_H_

#include <tsload/defs.h>

#include <tsload/pathutil.h>

#include <tsload/load/wlparam.h>

#include <iofile.h>

struct fileio_workload {
	wlp_string_t 	 path[PATHMAXLEN];
	wlp_integer_t 	 file_size;
	wlp_bool_t 		 sync;
	wlp_bool_t	 	 sparse;
};

struct diskio_workload {
	wlp_hiobject_t   disk;
	wlp_bool_t       force;
	wlp_bool_t 		 sync;
	wlp_bool_t 		 rdwr;
};

struct simpleio_request {
	wlp_strset_t rw;
	wlp_integer_t block_size;
	wlp_integer_t offset;
};

struct simpleio_wldata {
	io_file_t iof;
	uint64_t size;
	boolean_t do_remove;
};

#endif

