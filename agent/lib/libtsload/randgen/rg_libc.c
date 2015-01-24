
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

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

#include <tsload/load/randgen.h>

#include <stdlib.h>
#include <math.h>


/**
 * #### libc (default) random generator
 *
 * Uses srand()/rand() functions from standard C library */

int rg_init_libc(randgen_t* rg) {
	srand(rg->rg_seed);

	return 0;
}

uint64_t rg_generate_int_libc(randgen_t* rg) {
	return rand();
}

randgen_class_t rg_libc_class = {
	RG_CLASS_HEAD("libc", B_TRUE, RAND_MAX),

	SM_INIT(.rg_init, 		  rg_init_libc),
	SM_INIT(.rg_destroy, 	  rg_destroy_dummy),
	SM_INIT(.rg_generate_int, rg_generate_int_libc),
};



