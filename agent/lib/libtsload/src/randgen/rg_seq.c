/*
 * rg_sq.c
 *
 *  Created on: Jan 27, 2014
 *      Author: myaut
 */

#include <defs.h>
#include <randgen.h>
#include <mempool.h>

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


