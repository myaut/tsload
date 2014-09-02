/*
 * o_tpdisp.c
 *
 *  Created on: Sep 2, 2014
 *      Author: myaut
 */

#define NO_JSON

#include <tsobj.h>
#include <tpdisp.h>

#include "helpers.h"

#include <stdio.h>

#include <assert.h>

void test_tpd_bad(void) {
	TEST_PREAMBLE("[ ]");
	assert(tsobj_tp_disp_proc(node) == NULL);
}

void test_tpd_empty(void) {
	TEST_PREAMBLE("{ }");
	assert(tsobj_tp_disp_proc(node) == NULL);
}

void test_invalid_tpd_type_type(void) {
	TEST_PREAMBLE("{ " JSON_PROP("type", 1.1) " }");
	assert(tsobj_tp_disp_proc(node) == NULL);
}

void test_invalid_tpd_type_value(void) {
	TEST_PREAMBLE("{ " JSON_PROP("type", "not a valid tpd type") " }");
	assert(tsobj_tp_disp_proc(node) == NULL);
}

void test_tpd_rr(void) {
	tp_disp_t* disp;
	TEST_PREAMBLE("{ " JSON_PROP("type", "round-robin") " }");

	disp = tsobj_tp_disp_proc(node);
	assert(disp != NULL);

	tpd_destroy(disp);
}

void test_tpd_fillup_invalid_n_type(void) {
	TEST_PREAMBLE("{ " JSON_PROP("type", "fill-up") ",  "
					   JSON_PROP("n", "fill-up") " }");
	assert(tsobj_tp_disp_proc(node) == NULL);
}

void test_tpd_fillup(void) {
	tp_disp_t* disp;
	TEST_PREAMBLE("{ " JSON_PROP("type", "fill-up") ",  "
					   JSON_PROP("n", 10) ",  "
					   JSON_PROP("wid", 1)" }");

	disp = tsobj_tp_disp_proc(node);
	assert(disp != NULL);

	tpd_destroy(disp);
}

int tsload_test_main() {
	test_tpd_bad();
	test_tpd_empty();
	test_invalid_tpd_type_type();
	test_invalid_tpd_type_value();

	test_tpd_rr();

	test_tpd_fillup_invalid_n_type();
	test_tpd_fillup();

	return 0;
}
