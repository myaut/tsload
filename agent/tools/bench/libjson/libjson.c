/*
 * libjson.c
 *
 *  Created on: Apr 27, 2014
 *      Author: myaut
 */

#include <mempool.h>
#include <tstime.h>

#include <stdlib.h>
#include <stdio.h>

#include <libjson.h>

void libjson_walk(JSONNODE* node) {
	boolean_t is_array = json_type(node) == JSON_ARRAY;
	JSONNODE_ITERATOR i_begin, i_end;
	JSONNODE* child;
	char* name;

	i_end = json_end(node);

	for(i_begin = json_begin(node); i_begin != i_end; ++i_begin) {
		child = *i_begin;
		if(!is_array) {
			name = json_name(child);
			json_free(name);
		}

		if(json_type(child) == JSON_ARRAY || json_type(child) == JSON_NODE) {
			libjson_walk(child);
		}
	}
}

void libjson_parse_bench(char* data, int iterations) {
	JSONNODE** nodes = malloc(sizeof(JSONNODE*) * iterations);

	int i;
	ts_time_t start;
	ts_time_t stop;
	ts_time_t delta;
	ts_time_t iter;
	char str1[40];
	char str2[40];

	start = tm_get_clock();

	for(i = 0; i < iterations; ++i) {
		nodes[i] = json_parse(data);
		libjson_walk(nodes[i]);
	}

	stop = tm_get_clock();

	delta = tm_diff(start, stop);
	tm_human_print(delta, str1, 40);

	iter = delta / iterations;
	tm_human_print(iter, str2, 40);

	for(i = 0; i < iterations; ++i) {
		json_delete(nodes[i]);
	}
	free(nodes);

	printf("Parsing took %s, %s per iteration\n", str1, str2);
}
