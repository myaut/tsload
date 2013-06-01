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

typedef struct steps_file {
	char	sf_wl_name[WLNAMELEN];

	FILE* 	sf_file;
	long 	sf_step_id;

	struct steps_file* sf_next;

	boolean_t sf_error;
} steps_file_t;

int step_open(const char* wl_name, const char* file_name);
int step_get_step(const char* wl_name, long* step_id, unsigned* p_num_rqs);
void step_close_all(void);

int steps_init(void);
void step_fini(void);

#endif /* STEPS_H_ */
