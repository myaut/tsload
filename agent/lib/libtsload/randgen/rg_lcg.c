
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

#include <tsload/mempool.h>

#include <tsload/load/randgen.h>


/**
 * #### Linear Congruential Generator
 *
 * See [w:Linear Congruential Generator](http://en.wikipedia.org/wiki/Linear_congruential_generator)
 * Uses MMIX constants, but tunable.
 *
 * Unlike libc generator (that is most likely LCG too), non-singleton, so it
 * provides independent streams of pseudo-random numbers.
 * */

uint64_t rg_lcg_multiplier = 6364136223846793005ll;
uint64_t rg_lcg_increment = 1442695040888963407ll;

typedef struct rq_lcg {
	uint64_t lcg_seed;
} rq_lcg_t;

int rg_init_lcg(randgen_t* rg) {
	rq_lcg_t* lcg = mp_malloc(sizeof(rq_lcg_t));

	if(rg->rg_seed == 0)
		rg->rg_seed = 1;

	lcg->lcg_seed = rg->rg_seed;
	rg->rg_private = lcg;

	return 0;
}

void rg_destroy_lcg(randgen_t* rg) {
	mp_free(rg->rg_private);
}

uint64_t rg_generate_int_lcg(randgen_t* rg) {
	rq_lcg_t* lcg = (rq_lcg_t*) rg->rg_private;

	lcg->lcg_seed = rg_lcg_multiplier * lcg->lcg_seed + rg_lcg_increment;

	return lcg->lcg_seed;
}

randgen_class_t rg_lcg_class = {
	RG_CLASS_HEAD("lcg", B_FALSE, ULLONG_MAX),

	SM_INIT(.rg_init, 		  rg_init_lcg),
	SM_INIT(.rg_destroy, 	  rg_destroy_lcg),
	SM_INIT(.rg_generate_int, rg_generate_int_lcg),
};




