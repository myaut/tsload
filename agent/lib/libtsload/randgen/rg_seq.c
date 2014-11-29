
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

#include <limits.h>


/**
 *
 * #### Sequental generator
 *
 * Uses API of random generators, but not really _random_ generator.
 * Generates sequence with start value rg_seed and in interval \[0; ULLONG_MAX)
 * */

typedef struct rq_seq {
	uint64_t value;
} rq_seq_t;

int rg_init_seq(randgen_t* rg) {
	rq_seq_t* seq = mp_malloc(sizeof(rq_seq_t));

	seq->value = rg->rg_seed;
	rg->rg_private = seq;

	return 0;
}

void rg_destroy_seq(randgen_t* rg) {
	mp_free(rg->rg_private);
}

uint64_t rg_generate_int_seq(randgen_t* rg) {
	rq_seq_t* seq = (rq_seq_t*) rg->rg_private;

	return seq->value++;
}

randgen_class_t rg_seq_class = {
	RG_CLASS_HEAD(B_FALSE, ULLONG_MAX),

	SM_INIT(.rg_init, 		  rg_init_seq),
	SM_INIT(.rg_destroy, 	  rg_destroy_seq),
	SM_INIT(.rg_generate_int, rg_generate_int_seq),
};



