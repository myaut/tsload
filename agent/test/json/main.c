/*
 * main.c
 *
 *  Created on: Apr 26, 2014
 *      Author: myaut
 */

#include <mempool.h>
#include <threads.h>

#include <json.h>

#include <stdio.h>

extern int json_test_main(void);

void dump_error(void) {
	json_error_state_t* state = json_get_error();

	if(state) {
		fprintf(stderr, "%s at %d:%d\n", state->je_message, state->je_line, state->je_char);
	}
}

void dump_node(json_node_t* node) {
	switch(node->jn_type) {
	case JSON_STRING:
		fprintf(stderr, "STRING: '%s'\n", json_as_string(node));
		break;
	}
}

int test_main(void) {
	int ret;

	threads_init();
	mempool_init();
	json_init();

	json_test_main();

	json_fini();
	mempool_fini();
	threads_fini();

	return 0;
}
