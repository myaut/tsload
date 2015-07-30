/*
 * o_rqsched.c
 *
 *  Created on: Sep 2, 2014
 *      Author: myaut
 */

#include <tsload/defs.h>

#include <tsload/mempool.h>
#include <tsload/autostring.h>

#include <tsload/obj/obj.h>

#include <tsload/load/workload.h>
#include <tsload/load/rqsched.h>

#include <assert.h>

#include "helpers.h"

workload_t* wl;

void test_rqsched_bad(void) {
	TEST_PREAMBLE("[ ]");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_BAD);
	assert(wl->wl_rqsched_class == NULL);
	assert(wl->wl_rqsched_private == NULL);
	json_node_destroy(node);
}

void test_rqsched_empty() {
	TEST_PREAMBLE(" { } ");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_BAD);
	assert(wl->wl_rqsched_class == NULL);
	assert(wl->wl_rqsched_private == NULL);
	json_node_destroy(node);
}

void test_rqsched_invalid_type_type() {
	TEST_PREAMBLE(" { " JSON_PROP2("type", "[]")  " } ");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_BAD);
	assert(wl->wl_rqsched_class == NULL);
	assert(wl->wl_rqsched_private == NULL);
	json_node_destroy(node);
}

void test_rqsched_invalid_type_value() {
	TEST_PREAMBLE(" { " JSON_PROP("type", "invalid_type")  " } ");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_ERROR);
	assert(wl->wl_rqsched_class == NULL);
	assert(wl->wl_rqsched_private == NULL);
	json_node_destroy(node);
}

void test_rqsched_simple_unused() {
	TEST_PREAMBLE(" { " JSON_PROP("type", "simple")  ", "
			 	 	    JSON_PROP("distribution", "uniform") " } ");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_BAD);
	assert(wl->wl_rqsched_class == NULL);
	assert(wl->wl_rqsched_private == NULL);
	json_node_destroy(node);
}

void test_rqsched_simple() {
	TEST_PREAMBLE(" { " JSON_PROP("type", "simple") " } ");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_OK);
	assert(wl->wl_rqsched_class == rqsched_find("simple"));

	rqsched_destroy(wl);
	json_node_destroy(node);
}

void test_rqsched_iat_no_dist() {
	TEST_PREAMBLE(" { " JSON_PROP("type", "iat") " } ");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_BAD);
	assert(wl->wl_rqsched_class == NULL);
	assert(wl->wl_rqsched_private == NULL);
	json_node_destroy(node);
}

void test_rqsched_iat_invalid_dist_type() {
	TEST_PREAMBLE(" { " JSON_PROP("type", "iat") ", "
						JSON_PROP("distribution", true) " } ");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_BAD);
	assert(wl->wl_rqsched_class == NULL);
	assert(wl->wl_rqsched_private == NULL);
	json_node_destroy(node);
}

void test_rqsched_iat_invalid_dist_value() {
	TEST_PREAMBLE(" { " JSON_PROP("type", "iat") ", "
						JSON_PROP("distribution", "autumn") " } ");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_ERROR);
	assert(wl->wl_rqsched_class == NULL);
	assert(wl->wl_rqsched_private == NULL);
	json_node_destroy(node);
}

void test_rqsched_iat_no_parameter() {
	TEST_PREAMBLE(" { " JSON_PROP("type", "iat") ", "
						JSON_PROP("distribution", "uniform") " } ");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_BAD);
	assert(wl->wl_rqsched_class == NULL);
	assert(wl->wl_rqsched_private == NULL);
	json_node_destroy(node);
}

void test_rqsched_iat_invalid_scope_value() {
	TEST_PREAMBLE(" { " JSON_PROP("type", "iat") ", "
						JSON_PROP("distribution", "uniform")  ", "
						JSON_PROP("scope", -100.0) " } ");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_ERROR);
	assert(wl->wl_rqsched_class == NULL);
	assert(wl->wl_rqsched_private == NULL);
	json_node_destroy(node);
}

void test_rqsched_iat_invalid_randgen() {
	TEST_PREAMBLE(" { " JSON_PROP("type", "iat") ", "
						JSON_PROP2("randgen", " { } ") ", "
						JSON_PROP("distribution", "uniform")  ", "
						JSON_PROP("scope", 0.3) " } ");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_RG_ERROR);
	assert(wl->wl_rqsched_class == NULL);
	assert(wl->wl_rqsched_private == NULL);
	json_node_destroy(node);
}

void test_rqsched_iat() {
	TEST_PREAMBLE(" { " JSON_PROP("type", "iat") ", "
						JSON_PROP2("randgen", " { "
								JSON_PROP("class", "lcg") " } ") ", "
						JSON_PROP("distribution", "uniform")  ", "
						JSON_PROP("scope", 0.3) " } ");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_OK);

	assert(wl->wl_rqsched_class == rqsched_find("iat"));
	assert(wl->wl_rqsched_private != NULL);
	json_node_destroy(node);

	rqsched_destroy(wl);
}

void test_rqsched_iat_unused() {
	TEST_PREAMBLE(" { " JSON_PROP("type", "iat") ", "
						JSON_PROP("distribution", "uniform")  ", "
						JSON_PROP("scope", 0.2) ", "
						JSON_PROP("min", 10.0) " } ");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_BAD);
	assert(wl->wl_rqsched_class == NULL);
	assert(wl->wl_rqsched_private == NULL);
	json_node_destroy(node);
}

void test_rqsched_think_no_params() {
	TEST_PREAMBLE(" { " JSON_PROP("type", "think") ", "
						JSON_PROP("distribution", "exponential") " } ");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_BAD);
	assert(wl->wl_rqsched_class == NULL);
	assert(wl->wl_rqsched_private == NULL);
	json_node_destroy(node);
}

void test_rqsched_think_invalid_users_type() {
	TEST_PREAMBLE(" { " JSON_PROP("type", "think") ", "
						JSON_PROP("distribution", "exponential") ", "
						JSON_PROP2("nusers", " { }") " } ");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_BAD);
	assert(wl->wl_rqsched_class == NULL);
	assert(wl->wl_rqsched_private == NULL);
	json_node_destroy(node);
}

void test_rqsched_think_invalid_users_value() {
	TEST_PREAMBLE(" { " JSON_PROP("type", "think") ", "
						JSON_PROP("distribution", "exponential") ", "
						JSON_PROP("nusers", -10) " } ");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_ERROR);
	assert(wl->wl_rqsched_class == NULL);
	assert(wl->wl_rqsched_private == NULL);
	json_node_destroy(node);
}

void test_rqsched_think() {
	TEST_PREAMBLE(" { " JSON_PROP("type", "think") ", "
						JSON_PROP("distribution", "exponential") ", "
						JSON_PROP("nusers", 4) " } ");
	assert(tsobj_rqsched_proc(node, wl) == RQSCHED_TSOBJ_OK);
	assert(wl->wl_rqsched_class == rqsched_find("think"));
	assert(wl->wl_rqsched_private != NULL);

	rqsched_destroy(wl);
	json_node_destroy(node);
}

int tsload_test_main() {
	wl = mp_malloc(sizeof(workload_t));

	aas_copy(aas_init(&wl->wl_name), "o_rqsched");
	wl->wl_rqsched_class = NULL;
	wl->wl_rqsched_private = NULL;

	test_rqsched_bad();
	test_rqsched_empty();
	test_rqsched_invalid_type_type();
	test_rqsched_invalid_type_value();

	test_rqsched_simple();
	test_rqsched_simple_unused();

	test_rqsched_iat_no_dist();
	test_rqsched_iat_invalid_dist_type();
	test_rqsched_iat_invalid_dist_value();
	test_rqsched_iat_no_parameter();
	test_rqsched_iat_invalid_scope_value();
	test_rqsched_iat_invalid_randgen();
	test_rqsched_iat();
	test_rqsched_iat_unused();

	test_rqsched_think_no_params();
	test_rqsched_think_invalid_users_type();
	test_rqsched_think_invalid_users_value();
	test_rqsched_think();

	aas_free(&wl->wl_name);
	mp_free(wl);

	return 0;
}
