/*
 * steps.c
 *
 *  Created on: 10.01.2013
 *      Author: myaut
 */

#include <steps.h>
#include <workload.h>
#include <hashmap.h>
#include <mempool.h>
#include <plat/posixdecl.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

/* TODO: Implement TSFile steps */

int step_get_step_file(steps_file_t* sf, long* step_id, unsigned* p_num_rqs);
int step_get_step_const(steps_const_t* sc, long* step_id, unsigned* p_num_rqs);

steps_generator_t* step_create_generator(steps_generator_type_t type) {
	steps_generator_t* sg;

	sg = mp_malloc(sizeof(steps_generator_t));
	sg->sg_type = type;

	return sg;
}

steps_generator_t* step_create_file(const char* file_name, const char* out_file_name) {
	steps_generator_t* sg;
	steps_file_t* sf;
	FILE* file = fopen(file_name, "r");
	FILE* fileout;
	struct stat statbuf;

	if(!file)
		return NULL;

	if(stat(file, &statbuf) == 0) {
		fclose(file);
		return NULL;
	}

	fileout = fopen(out_file_name, "w");

	if(!fileout) {
		fclose(file);
		return NULL;
	}

	sg = step_create_generator(STEPS_FILE);

	sf = &sg->sg_file;
	sf->sf_file = file;
	sf->sf_file_out = fileout;
	sf->sf_step_id = 0;
	sf->sf_error = B_FALSE;

	return sg;
}

steps_generator_t* step_create_const(long num_steps, unsigned num_requests) {
	steps_generator_t* sg;

	if(num_steps < 0)
		return NULL;

	sg = step_create_generator(STEPS_CONST);

	sg->sg_const.sc_step_id = 0;
	sg->sg_const.sc_num_steps = num_steps;
	sg->sg_const.sc_num_requests = num_requests;

	return sg;
}

static long step_parse_line(char* line) {
	/* Check if all characters in string are digits*/
	char* p = line;
	while(*p) {
		if(isspace(*p)) {
			*p = '\0';
			break;
		}

		if(!isdigit(*p))
			return STEP_ERROR;

		++p;
	}

	/* Read and process number of requests
	 * No need for checking of negative values,
	 * because we filter minus sign earlier*/
	return atol(line);
}

void step_close_file(steps_file_t* sf) {
	fclose(sf->sf_file);
	fclose(sf->sf_file_out);
}

int step_get_step(steps_generator_t* sg, long* step_id, unsigned* p_num_rqs) {
	switch(sg->sg_type) {
	case STEPS_FILE:
		return step_get_step_file(&sg->sg_file, step_id, p_num_rqs);
	case STEPS_CONST:
		return step_get_step_const(&sg->sg_const, step_id, p_num_rqs);
	}

	return STEP_ERROR;
}

int step_get_step_file(steps_file_t* sf, long* step_id, unsigned* p_num_rqs) {
	char step_str[16];
	char* p;
	long num_rqs;

	*step_id = sf->sf_step_id;

	/* There are no more steps on file */
	if(sf->sf_error || feof(sf->sf_file) != 0) {
		sf->sf_error = B_TRUE;
		return STEP_NO_RQS;
	}

	/* Read next step from file */
	fgets(step_str, 16, sf->sf_file);

	num_rqs = step_parse_line(step_str);

	if(num_rqs < 0) {
		sf->sf_error = B_TRUE;
		return STEP_ERROR;
	}

	*p_num_rqs = num_rqs;

	sf->sf_step_id++;

	fprintf(sf->sf_file_out, "%ld\n", num_rqs);

	return STEP_OK;
}

int step_get_step_const(steps_const_t* sc, long* step_id, unsigned* p_num_rqs) {
	if(sc->sc_step_id == sc->sc_num_steps) {
		return STEP_NO_RQS;
	}

	*step_id = sc->sc_step_id;
	*p_num_rqs = sc->sc_num_requests;

	sc->sc_step_id++;

	return STEP_OK;
}

void step_destroy(steps_generator_t* sg) {
	if(sg->sg_type == STEPS_FILE)
		step_close_file(&sg->sg_file);

	mp_free(sg);
}
