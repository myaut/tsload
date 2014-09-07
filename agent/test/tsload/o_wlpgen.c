/*
 * o_wlpgen.c
 *
 *  Created on: Sep 6, 2014
 *      Author: myaut
 */

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

#define PMAP_TEST_PREAMBLE(propname, pmap)		\
	WLP_TEST_PREAMBLE(propname,					\
		"{ " JSON_PROP2(propname, "{ "			\
					JSON_RANDGEN ", "			\
					JSON_PROP2("pmap", pmap)	\
				" }") " }");

#define JSON_RANDGEN		JSON_PROP2("randgen", "{" JSON_PROP("class", "lcg") "}")
#define JSON_RANDVAR		JSON_PROP2("randvar", 	\
		"{" JSON_PROP("class", "exponential") ", " 	\
			JSON_PROP("rate", 0.1) "}")

wlp_descr_t int_param = {
	WLP_INTEGER, WLPF_REQUEST,
	WLP_INT_RANGE(-10, 100),
	WLP_NO_DEFAULT(),
	"i", "",
	offsetof(struct test_data, i)
};

wlp_descr_t string_param = {
	WLP_RAW_STRING, WLPF_REQUEST,
	WLP_STRING_LENGTH(32),
	WLP_NO_DEFAULT(),
	"s", "",
	offsetof(struct test_data, s)
};

void test_int_param_invalid_type(void) {
	WLP_TEST_PREAMBLE("i",
		"{ " JSON_PROP("i", "string") " }");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_BAD);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_int_param_empty(void) {
	WLP_TEST_PREAMBLE("i",
		"{ " JSON_PROP2("i", "{ "
				" }") " }");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_BAD);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_int_invalid_randgen_node(void) {
	WLP_TEST_PREAMBLE("i",
		"{ " JSON_PROP2("i", "{ "
					JSON_PROP("randgen", "not a random generator")
				" }") " }");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_BAD);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_int_invalid_randgen_class(void) {
	WLP_TEST_PREAMBLE("i",
		"{ " JSON_PROP2("i", "{ "
					JSON_PROP2("randgen",
						"{" JSON_PROP("class", "invalid class") "}")
				" }") " }");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_ERROR);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_int_invalid_randvar_node(void) {
	WLP_TEST_PREAMBLE("i",
		"{ " JSON_PROP2("i", "{ "
					JSON_RANDGEN ", "
					JSON_PROP("randvar", "not a random generator")
				" }") " }");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_BAD);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_int_invalid_randvar_class(void) {
	WLP_TEST_PREAMBLE("i",
		"{ " JSON_PROP2("i", "{ "
					JSON_RANDGEN ", "
					JSON_PROP2("randvar",
						"{" JSON_PROP("class", "invalid class") "}")
				" }") " }");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_ERROR);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_int_unused(void) {
	WLP_TEST_PREAMBLE("i",
		"{ " JSON_PROP2("i", "{ "
					JSON_RANDGEN ", "
					JSON_PROP("unused", 10)
				" }") " }");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_BAD);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_int_randvar_and_pmap(void) {
	WLP_TEST_PREAMBLE("i",
		"{ " JSON_PROP2("i", "{ "
				JSON_RANDGEN ", "
				JSON_RANDVAR ", "
				JSON_PROP2("pmap", "["
								"{"
								 JSON_PROP("probability", 1.0) ", "
								 JSON_PROP("value", 1)
							   "}"
							   "]")
				" }") " }");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_BAD);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_int_randvar_ok(void) {
	WLP_TEST_PREAMBLE("i",
		"{ " JSON_PROP2("i", "{ "
					JSON_RANDGEN ", "
					JSON_RANDVAR
				" }") " }");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_OK);
	assert(!list_empty(&wl->wl_wlpgen_head));
	wlpgen_destroy_all(wl);

	json_node_destroy(node);
}

void test_int_randgen_ok(void) {
	WLP_TEST_PREAMBLE("i",
		"{ " JSON_PROP2("i", "{ "
					JSON_RANDGEN
				" }") " }");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_OK);
	assert(!list_empty(&wl->wl_wlpgen_head));
	wlpgen_destroy_all(wl);

	json_node_destroy(node);
}

void test_int_pmap_empty(void) {
	PMAP_TEST_PREAMBLE("i", "[]");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_BAD);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_int_pmap_invalid_class(void) {
	PMAP_TEST_PREAMBLE("i", "\"not a pmap\"");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_BAD);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_int_pmap_no_probability(void) {
	PMAP_TEST_PREAMBLE("i", "["
				"{}"
			"]");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_BAD);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_int_pmap_no_value(void) {
	PMAP_TEST_PREAMBLE("i", "["
				"{"
					JSON_PROP("probability", 1.0)
				"}"
			"]");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_BAD);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}


void test_int_pmap_value_and_valaray(void) {
	PMAP_TEST_PREAMBLE("i", "["
				"{"
					JSON_PROP("probability", 1.0) ", "
					JSON_PROP("value", 1) ", "
					JSON_PROP("valarray", "[]")
				"}"
			"]");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_BAD);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_int_pmap_invalid_value(void) {
	PMAP_TEST_PREAMBLE("i", "["
				"{"
					JSON_PROP("probability", 1.0) ", "
					JSON_PROP("value", 3.14)
				"}"
			"]");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_BAD);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_int_pmap_small_probability(void) {
	PMAP_TEST_PREAMBLE("i", "["
				"{"
					JSON_PROP("probability", 0.5) ", "
					JSON_PROP("value", 1)
				"}, "
				"{"
					JSON_PROP("probability", 0.4999) ", "
					JSON_PROP("value", 2)
				"} "
			"]");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_ERROR);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_int_pmap_normal_probability(void) {
	PMAP_TEST_PREAMBLE("i", "["
				"{"
					JSON_PROP("probability", 0.3) ", "
					JSON_PROP("value", 1)
				"}, "
				"{"
					JSON_PROP("probability", 0.3) ", "
					JSON_PROP("value", 2)
				"}, "
				"{"
					JSON_PROP("probability", 0.3) ", "
					JSON_PROP("value", 3)
				"}, "
				"{"
					JSON_PROP("probability", 0.1) ", "
					JSON_PROP("value", 10)
				"}, "
			"]");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_OK);
	assert(!list_empty(&wl->wl_wlpgen_head));
	wlpgen_destroy_all(wl);

	json_node_destroy(node);
}

void test_int_pmap_big_probability(void) {
	PMAP_TEST_PREAMBLE("i", "["
				"{"
					JSON_PROP("probability", 0.5) ", "
					JSON_PROP("value", 1)
				"}, "
				"{"
					JSON_PROP("probability", 0.5001) ", "
					JSON_PROP("value", 2)
				"} "
			"]");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_ERROR);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_int_pmap_empty_valarray(void) {
	PMAP_TEST_PREAMBLE("i", "["
				"{"
					JSON_PROP("probability", 0.5) ", "
					JSON_PROP2("valarray", "[]")
				"}, "
				"{"
					JSON_PROP("probability", 0.5) ", "
					JSON_PROP("value", 2)
				"} "
			"]");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_BAD);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_int_pmap_valarray_invalid_node(void) {
	PMAP_TEST_PREAMBLE("i", "["
				"{"
					JSON_PROP("probability", 0.5) ", "
					JSON_PROP2("valarray", "[ 10, true ]")
				"}, "
				"{"
					JSON_PROP("probability", 0.5) ", "
					JSON_PROP("value", 2)
				"} "
			"]");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_BAD);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_int_pmap_valarray_ok(void) {
	PMAP_TEST_PREAMBLE("i", "["
				"{"
					JSON_PROP("probability", 0.4) ", "
					JSON_PROP2("valarray", "[ 10, 30 ]")
				"}, "
				"{"
					JSON_PROP("probability", 0.6) ", "
					JSON_PROP("value", 2)
				"} "
			"]");

	assert(tsobj_wlpgen_proc(param, &int_param, wl)
				== WLPARAM_TSOBJ_OK);
	assert(!list_empty(&wl->wl_wlpgen_head));
	wlpgen_destroy_all(wl);

	json_node_destroy(node);
}

void test_string_randvar(void) {
	WLP_TEST_PREAMBLE("s",
		"{ " JSON_PROP2("s", "{ "
					JSON_RANDGEN ", "
					JSON_RANDVAR
				" }") " }");

	assert(tsobj_wlpgen_proc(param, &string_param, wl)
				== WLPARAM_TSOBJ_BAD);
	assert(list_empty(&wl->wl_wlpgen_head));

	json_node_destroy(node);
}

void test_string_pmap_ok(void) {
	PMAP_TEST_PREAMBLE("s", "["
				"{"
					JSON_PROP("probability", 0.4) ", "
					JSON_PROP2("valarray", "[ \"a\", \"b\" ]")
				"}, "
				"{"
					JSON_PROP("probability", 0.6) ", "
					JSON_PROP("value", "c")
				"} "
			"]");

	assert(tsobj_wlpgen_proc(param, &string_param, wl)
				== WLPARAM_TSOBJ_OK);
	assert(!list_empty(&wl->wl_wlpgen_head));
	wlpgen_destroy_all(wl);

	json_node_destroy(node);
}


int tsload_test_main() {
	wl = mp_malloc(sizeof(workload_t));
	strcpy(wl->wl_name, "o_wlpgen");
	list_head_init(&wl->wl_wlpgen_head, "wlpgen_head");


	test_int_param_invalid_type();
	test_int_param_empty();

	test_int_invalid_randgen_node();
	test_int_invalid_randgen_class();

	test_int_invalid_randvar_node();
	test_int_invalid_randvar_class();

	test_int_unused();

	test_int_randvar_and_pmap();
	test_int_randvar_ok();
	test_int_randgen_ok();

	test_int_pmap_empty();
	test_int_pmap_invalid_class();
	test_int_pmap_no_probability();
	test_int_pmap_no_value();
	test_int_pmap_value_and_valaray();
	test_int_pmap_invalid_value();
	test_int_pmap_small_probability();
	test_int_pmap_normal_probability();
	test_int_pmap_big_probability();
	test_int_pmap_empty_valarray();
	test_int_pmap_valarray_invalid_node();
	test_int_pmap_valarray_ok();

	test_string_randvar();
	test_string_pmap_ok();

	mp_free(wl);

	return 0;
}
