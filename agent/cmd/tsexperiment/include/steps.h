
/*
    This file is part of TSLoad.
    Copyright 2013-2014, Sergey Klyaus, ITMO University

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



#ifndef STEPS_H_
#define STEPS_H_

#include <defs.h>
#include <list.h>

#include <experiment.h>

#include <stdio.h>

#define STEP_OK			0
#define STEP_NO_RQS		-1
#define STEP_ERROR 		-2

struct steps_generator;

typedef enum {
	STEPS_FILE,
	STEPS_CONST,
	STEPS_TRACE
} steps_generator_type_t;

typedef struct steps_file {
	FILE* 	sf_file;
	FILE*   sf_file_out;
	long 	sf_step_id;

	boolean_t sf_error;
} steps_file_t;

typedef struct steps_const {
	unsigned sc_num_steps;
	unsigned sc_step_id;

	unsigned sc_num_requests;
} steps_const_t;

typedef struct steps_trace {
	struct steps_generator* st_parent;

	experiment_t* st_exp;
	exp_workload_t* st_ewl;

	list_head_t st_wl_chain;
} steps_trace_t;

typedef struct steps_generator {
	steps_generator_type_t sg_type;

	struct  steps_generator* sg_next;

	union {
		steps_file_t sg_file;
		steps_const_t sg_const;
		steps_trace_t sg_trace;
	};
} steps_generator_t;

steps_generator_t* step_create_file(const char* file_name, const char* out_file_name);
steps_generator_t* step_create_const(long num_steps, unsigned num_requests);
steps_generator_t* step_create_trace(steps_generator_t* parent, experiment_t* base, exp_workload_t* ewl);

int step_get_step(steps_generator_t* sg, long* step_id, unsigned* p_num_rqs, list_head_t* trace_rqs);

void step_destroy(steps_generator_t* sg);

#endif /* STEPS_H_ */


