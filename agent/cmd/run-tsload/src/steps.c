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

DECLARE_HASH_MAP(steps_hash_map, steps_file_t, WLHASHSIZE, sf_wl_name, sf_next,
	{
		return hm_string_hash(key, WLHASHMASK);
	},
	{
		return strcmp((char*) key1, (char*) key2) == 0;
	}
)

int step_open(const char* wl_name, const char* file_name) {
	steps_file_t* sf;
	FILE* file = fopen(file_name, "r");

	if(!file)
		return STEP_ERROR;

	sf = mp_malloc(sizeof(steps_file_t));

	sf->sf_file = file;
	sf->sf_next = NULL;
	sf->sf_step_id = 0;
	sf->sf_error = B_FALSE;
	strncpy(sf->sf_wl_name, wl_name, WLNAMELEN);

	hash_map_insert(&steps_hash_map, (hm_item_t*) sf);

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

void step_close(steps_file_t* sf) {
	fclose(sf->sf_file);
	mp_free(sf);
}

int step_get_step(const char* wl_name, long* step_id, unsigned* p_num_rqs) {
	steps_file_t* sf = (steps_file_t*) hash_map_find(&steps_hash_map, wl_name);

	char step_str[16];
	char* p;
	long num_rqs;

	if(sf == NULL)
		return STEP_ERROR;

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

int step_close_walker(void* psf, void* arg) {
	steps_file_t* sf = (steps_file_t*) psf;

	step_close(sf);

	return HM_WALKER_CONTINUE | HM_WALKER_REMOVE;
}

void step_close_all(void) {
	hash_map_walk(&steps_hash_map, step_close_walker, NULL);
}

int steps_init(void) {
	hash_map_init(&steps_hash_map, "steps_hash_map");

	return 0;
}

void step_fini(void) {
	step_close_all();
	hash_map_destroy(&steps_hash_map);
}
