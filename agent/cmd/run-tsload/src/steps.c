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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

DECLARE_HASH_MAP(steps_hash_map, steps_generator_t, WLHASHSIZE, sg_wl_name, sg_next,
	{
		return hm_string_hash(key, WLHASHMASK);
	},
	{
		return strcmp((char*) key1, (char*) key2) == 0;
	}
)

/* TODO: Implement TSFile steps */

int step_get_step_file(steps_file_t* sf, long* step_id, unsigned* p_num_rqs);
int step_get_step_const(steps_const_t* sc, long* step_id, unsigned* p_num_rqs);

steps_generator_t* step_create_generator(steps_generator_type_t type, const char* wl_name) {
	steps_generator_t* sg;

	sg = mp_malloc(sizeof(steps_generator_t));

	sg->sg_type = type;
	sg->sg_next = NULL;
	strncpy(sg->sg_wl_name, wl_name, WLNAMELEN);

	hash_map_insert(&steps_hash_map, (hm_item_t*) sg);

	return sg;
}

int step_create_file(const char* wl_name, const char* file_name) {
	steps_generator_t* sg;
	steps_file_t* sf;
	FILE* file = fopen(file_name, "r");

	if(!file)
		return STEP_ERROR;

	sg = step_create_generator(STEPS_FILE, wl_name);

	sf = &sg->sg_file;
	sf->sf_file = file;
	sf->sf_step_id = 0;
	sf->sf_error = B_FALSE;

	return STEP_OK;
}

int step_create_const(const char* wl_name, long num_steps, unsigned num_requests) {
	steps_generator_t* sg;

	if(num_steps < 0)
		return STEP_ERROR;

	sg = step_create_generator(STEPS_CONST, wl_name);

	sg->sg_const.sc_step_id = 0;
	sg->sg_const.sc_num_steps = num_steps;
	sg->sg_const.sc_num_requests = num_requests;

	return STEP_OK;
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
}

int step_get_step(const char* wl_name, long* step_id, unsigned* p_num_rqs) {
	steps_generator_t* sg = (steps_generator_t*) hash_map_find(&steps_hash_map, wl_name);

	if(sg == NULL)
		return STEP_ERROR;

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

int step_destroy_walker(void* psg, void* arg) {
	steps_generator_t* sg = (steps_generator_t*) psg;

	if(sg->sg_type == STEPS_FILE)
		step_close_file(&sg->sg_file);

	mp_free(sg);

	return HM_WALKER_CONTINUE | HM_WALKER_REMOVE;
}

void step_destroy_all(void) {
	hash_map_walk(&steps_hash_map, step_destroy_walker, NULL);
}

int steps_init(void) {
	hash_map_init(&steps_hash_map, "steps_hash_map");

	return 0;
}

void steps_fini(void) {
	step_destroy_all();
	hash_map_destroy(&steps_hash_map);
}
