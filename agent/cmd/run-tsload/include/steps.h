/*
 * steps.h
 *
 *  Created on: 10.01.2013
 *      Author: myaut
 */

#ifndef STEPS_H_
#define STEPS_H_

#include <workload.h>

#include <stdio.h>

#define STEP_OK			0
#define STEP_NO_RQS		-1
#define STEP_ERROR 		-2

typedef enum {
	STEPS_FILE,
	STEPS_CONST
} steps_generator_type_t;

typedef struct steps_file {
	FILE* 	sf_file;
	long 	sf_step_id;

	boolean_t sf_error;
} steps_file_t;

typedef struct steps_const {
	unsigned sc_num_steps;
	unsigned sc_step_id;

	unsigned sc_num_requests;
} steps_const_t;

typedef struct steps_generator {
	steps_generator_type_t sg_type;

	char	sg_wl_name[WLNAMELEN];
	struct  steps_generator* sg_next;

	union {
		steps_file_t sg_file;
		steps_const_t sg_const;
	};
} steps_generator_t;

int step_create_file(const char* wl_name, const char* file_name);
int step_create_const(const char* wl_name, long num_steps, unsigned num_requests);
int step_get_step(const char* wl_name, long* step_id, unsigned* p_num_rqs);

int steps_init(void);
void steps_fini(void);

#endif /* STEPS_H_ */
