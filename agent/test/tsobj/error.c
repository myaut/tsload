/*
 * error.c
 *
 *  Created on: Sep 1, 2014
 *      Author: myaut
 */

#include <tsload/defs.h>

#include <tsload/mempool.h>

#include <tsload/json/json.h>

#include <tsload/obj/obj.h>

#include <stdio.h>
#include <assert.h>


void test_not_found() {
	tsobj_node_t* node = tsobj_new_node(NULL);
	int i;

	int error = tsobj_get_integer_i(node, "i", &i);

	assert(error == tsobj_errno());
	assert(error == TSOBJ_NOT_FOUND);

	printf("test_not_found: %s\n", tsobj_error_message());

	tsobj_node_destroy(node);
}

void test_invalid_type() {
	tsobj_node_t* node = tsobj_new_node(NULL);
	int i;

	int error;

	tsobj_add_string(node, TSOBJ_STR("i"), TSOBJ_STR("test"));

	error = tsobj_get_integer_i(node, "i", &i);

	assert(error == tsobj_errno());
	assert(error == TSOBJ_INVALID_TYPE);

	printf("test_invalid_type: %s\n", tsobj_error_message());

	tsobj_node_destroy(node);
}

void test_clear_error() {
	tsobj_node_t* node = tsobj_new_node(NULL);
	int i;

	int error;

	tsobj_add_integer(node, TSOBJ_STR("i"), 10);

	tsobj_errno_clear();

	error = tsobj_get_integer_i(node, "i", &i);

	assert(i == 10);
	assert(error == tsobj_errno());
	assert(error == TSOBJ_OK);

	printf("test_invalid_type: %s\n", tsobj_error_message());

	tsobj_node_destroy(node);
}

void run_all_tests() {
	test_not_found();
	test_invalid_type();
	test_clear_error();
}

int test_main(void) {
	int ret;

	threads_init();
	mempool_init();
	json_init();
	tsobj_init();

	run_all_tests();

	tsobj_fini();
	json_fini();
	mempool_fini();
	threads_fini();

	return 0;
}
