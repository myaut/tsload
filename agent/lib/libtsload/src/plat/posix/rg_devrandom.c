
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



#include <defs.h>
#include <randgen.h>
#include <mempool.h>

#include <plat/posixdecl.h>

#define DEV_RANDOM		"/dev/urandom"

/**
 * #### /dev/urandom generator
 *
 * Generates random numbers using /dev/urandom character device
 *
 * NOTE: Since /dev/random doesn't guarantee non-blocking, (and TSLoad is
 * not an cryptography software) it is not implemented
 *
 * NOTE: doesn't use seed
 * */

typedef struct rq_devrandom {
	int fd;
} rq_devrandom_t;

int rg_init_devrandom(randgen_t* rg) {
	rq_devrandom_t* dr = mp_malloc(sizeof(rq_devrandom_t));

	dr->fd = open(DEV_RANDOM, O_RDONLY);
	if(dr->fd == -1)
		return -1;

	rg->rg_private = dr;

	return 0;
}

void rg_destroy_devrandom(randgen_t* rg) {
	rq_devrandom_t* dr = (rq_devrandom_t*) rg->rg_private;

	close(dr->fd);

	mp_free(rg->rg_private);
}

uint64_t rg_generate_int_devrandom(randgen_t* rg) {
	rq_devrandom_t* dr = (rq_devrandom_t*) rg->rg_private;
	uint64_t value;

	read(dr->fd, &value, sizeof(uint64_t));

	return value;
}

randgen_class_t rg_devrandom_class = {
	RG_CLASS_HEAD(B_TRUE, ULLONG_MAX),

	SM_INIT(.rg_init, 		  rg_init_devrandom),
	SM_INIT(.rg_destroy, 	  rg_destroy_devrandom),
	SM_INIT(.rg_generate_int, rg_generate_int_devrandom),
};



