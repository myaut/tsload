/*
 * libtsjson.c
 *
 *  Created on: Apr 27, 2014
 *      Author: myaut
 */

#include <mempool.h>
#include <threads.h>
#include <tstime.h>

#include <json.h>

#include <stdio.h>

void tsjson_init(void) {
	mempool_init();
	threads_init();
	json_init();
}

void tsjson_fini(void) {
	json_fini();
	mempool_fini();
	threads_fini();
}

void tsjson_walk(json_node_t* node) {
	boolean_t is_array = json_type(node) == JSON_ARRAY;
	int id;
	json_node_t* child;
	char* name;
	volatile char c;

	json_for_each(node, child, id) {
		if(!is_array) {
			name = json_name(child);
			c = *name;
		}

		if(json_type(child) == JSON_ARRAY || json_type(child) == JSON_NODE) {
			tsjson_walk(child);
		}
	}
}

void tsjson_parse_bench(char* data, int iterations, size_t sz) {
	json_node_t** nodes = malloc(sizeof(json_node_t*) * iterations);

	int i;
	ts_time_t start;
	ts_time_t stop;
	ts_time_t delta;
	ts_time_t iter;
	char str1[40];
	char str2[40];

	long count;

	tsjson_init();

	start = tm_get_clock();

	for(i = 0; i < iterations; ++i) {
		json_parse(json_buf_create(data, sz, B_FALSE), &nodes[i]);
		tsjson_walk(nodes[i]);
	}

	stop = tm_get_clock();

	delta = tm_diff(start, stop);
	tm_human_print(delta, str1, 40);

	iter = delta / iterations;
	tm_human_print(iter, str2, 40);

	for(i = 0; i < iterations; ++i) {
		json_node_destroy(nodes[i]);
	}
	free(nodes);
	tsjson_fini();

	printf("Parsing took %s, %s per iteration\n", str1, str2);
}
