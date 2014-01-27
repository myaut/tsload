/*
 * rg_libc.c
 *
 *  Created on: Jan 25, 2014
 *      Author: myaut
 */

#include <randgen.h>

#include <stdlib.h>
#include <math.h>

/**
 * libc (default) random generator implementation
 * Uses srand()/rand() functions from standard C library */

int rg_init_libc(randgen_t* rg) {
	srand(rg->rg_seed);

	return 0;
}

uint64_t rg_generate_int_libc(randgen_t* rg) {
	return rand();
}

randgen_class_t rg_libc_class = {
	RG_CLASS_HEAD(B_TRUE, RAND_MAX),

	SM_INIT(.rg_init, 		  rg_init_libc),
	SM_INIT(.rg_destroy, 	  rg_destroy_dummy),
	SM_INIT(.rg_generate_int, rg_generate_int_libc),
};


