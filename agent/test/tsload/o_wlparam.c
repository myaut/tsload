/*
 * o_wlparam.c
 *
 *  Created on: Sep 3, 2014
 *      Author: myaut
 */

#define NO_JSON

#include <defs.h>
#include <tsobj.h>
#include <mempool.h>
#include <workload.h>
#include <wlparam.h>

#include "helpers.h"

#include <stdio.h>

#include <assert.h>

workload_t* wl;

struct test_data {
	wlp_integer_t i;
	wlp_string_t  s[32];
};

wlp_descr_t simple_params[] = {
	{ WLP_INTEGER, WLPF_NO_FLAGS,
		WLP_INT_RANGE(10, 100),
		WLP_NO_DEFAULT(),
		"i", "",
		offsetof(struct test_data, i)
	},
	{ WLP_RAW_STRING, WLPF_OPTIONAL,
		WLP_STRING_LENGTH(32),
		WLP_NO_DEFAULT(),
		"s", "",
		offsetof(struct test_data, s)
	},
	{ WLP_NULL }
};

void test_invalid_param_type(void) {
	struct test_data test_data;

	WLP_TEST_PREAMBLE("i",
		"{ " JSON_PROP("i", 1.1) " }");

	assert(tsobj_wlparam_proc(param, &simple_params[0], &test_data.i, wl)
			== WLPARAM_TSOBJ_BAD);
	json_node_destroy(node);
}

void test_int_param_smaller(void) {
	struct test_data test_data;

	WLP_TEST_PREAMBLE("i",
		"{ " JSON_PROP("i", -10) " }");

	assert(tsobj_wlparam_proc(param, &simple_params[0], &test_data.i, wl)
				== WLPARAM_OUTSIDE_RANGE);
	json_node_destroy(node);
}

void test_int_param_greater(void) {
	struct test_data test_data;

	WLP_TEST_PREAMBLE("i",
		"{ " JSON_PROP("i", 100500) " }");

	assert(tsobj_wlparam_proc(param, &simple_params[0], &test_data.i, wl)
				== WLPARAM_OUTSIDE_RANGE);
	json_node_destroy(node);
}

void test_int_param(void) {
	struct test_data test_data;

	WLP_TEST_PREAMBLE("i",
		"{ " JSON_PROP("i", 77) " }");

	assert(tsobj_wlparam_proc(param, &simple_params[0], &test_data.i, wl)
				== WLPARAM_TSOBJ_OK);
	json_node_destroy(node);
}

void test_str_param_loong(void) {
	struct test_data test_data;

	WLP_TEST_PREAMBLE("s",
		"{ " JSON_PROP("s", "loooooooooooooooooooooooooooooooong") " }");

	assert(tsobj_wlparam_proc(param, &simple_params[1], &test_data.s, wl)
				== WLPARAM_OUTSIDE_RANGE);
	json_node_destroy(node);
}

void test_str_param(void) {
	struct test_data test_data;

	WLP_TEST_PREAMBLE("s",
		"{ " JSON_PROP("s", "string") " }");

	assert(tsobj_wlparam_proc(param, &simple_params[1], &test_data.s, wl)
				== WLPARAM_TSOBJ_OK);
	json_node_destroy(node);
}

void test_simple_params_missing(void) {
	struct test_data test_data;

	TEST_PREAMBLE("{ " JSON_PROP("s", "string") " }");

	wl->wl_params = &test_data;
	assert(tsobj_wlparam_proc_all(node, simple_params, wl)
				== WLPARAM_TSOBJ_BAD);
	json_node_destroy(node);
}

void test_simple_params_unused(void) {
	struct test_data test_data;

	TEST_PREAMBLE("{ " JSON_PROP("s", "string") ", "
	 	 	   	   	   JSON_PROP("i", 11) ", "
	 	 	   	   	   JSON_PROP("z", 10.1)" }");

	wl->wl_params = &test_data;
	assert(tsobj_wlparam_proc_all(node, simple_params, wl)
				== WLPARAM_TSOBJ_BAD);
	json_node_destroy(node);
}

void test_simple_params(void) {
	struct test_data test_data;

	TEST_PREAMBLE("{ " JSON_PROP("s", "string") ", "
			 	 	   JSON_PROP("i", 11) "}");

	wl->wl_params = &test_data;
	assert(tsobj_wlparam_proc_all(node, simple_params, wl)
				== WLPARAM_TSOBJ_OK);
	json_node_destroy(node);
}

int tsload_test_main() {
	wl = mp_malloc(sizeof(workload_t));
	strcpy(wl->wl_name, "o_wlparam");

	/* Easy way to generate this:
	 * grep '^void' test/tsload/o_wlparam.c | sed 's/ {$/;/g;s/(void)/()/g;s/^void //g' */

	test_invalid_param_type();

	test_int_param_smaller();
	test_int_param_greater();
	test_int_param();

	test_str_param_loong();
	test_str_param();

	test_simple_params_unused();
	test_simple_params_missing();
	test_simple_params();

	mp_free(wl);

	return 0;
}
