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

void tpd_destroy_impl(tp_disp_t* tpd) {
	thread_pool_t tp;
	tp.tp_disp = tpd;
	tpd->tpd_tp = &tp;

	tpd_destroy(tpd);
}

void test_tpd_bad(void) {
	TEST_PREAMBLE("[ ]");
	assert(tsobj_tp_disp_proc(node) == NULL);
	json_node_destroy(node);
}

void test_tpd_empty(void) {
	TEST_PREAMBLE("{ }");
	assert(tsobj_tp_disp_proc(node) == NULL);
	json_node_destroy(node);
}

void test_invalid_tpd_type_type(void) {
	TEST_PREAMBLE("{ " JSON_PROP("type", 1.1) " }");
	assert(tsobj_tp_disp_proc(node) == NULL);
	json_node_destroy(node);
}

void test_invalid_tpd_type_value(void) {
	TEST_PREAMBLE("{ " JSON_PROP("type", "not a valid tpd type") " }");
	assert(tsobj_tp_disp_proc(node) == NULL);
	json_node_destroy(node);
}

void test_invalid_tpd_rr_unused(void) {
	TEST_PREAMBLE("{ " JSON_PROP("type", "round-robin") ",  "
					   JSON_PROP("wid", 1) " }");
	assert(tsobj_tp_disp_proc(node) == NULL);
	json_node_destroy(node);
}

void test_tpd_rr(void) {
	tp_disp_t* disp;
	TEST_PREAMBLE("{ " JSON_PROP("type", "round-robin") " }");

	disp = tsobj_tp_disp_proc(node);
	assert(disp != NULL);

	tpd_destroy_impl(disp);
	json_node_destroy(node);
}

void test_tpd_fillup_invalid_n_type(void) {
	TEST_PREAMBLE("{ " JSON_PROP("type", "fill-up") ",  "
					   JSON_PROP("n", "fill-up") " }");
	assert(tsobj_tp_disp_proc(node) == NULL);
	json_node_destroy(node);
}

void test_tpd_fillup(void) {
	tp_disp_t* disp;
	TEST_PREAMBLE("{ " JSON_PROP("type", "fill-up") ",  "
					   JSON_PROP("n", 10) ",  "
					   JSON_PROP("wid", 1)" }");

	disp = tsobj_tp_disp_proc(node);
	assert(disp != NULL);

	tpd_destroy_impl(disp);
	json_node_destroy(node);
}

void test_tpd_fillup_unused(void) {
	TEST_PREAMBLE("{ " JSON_PROP("type", "fill-up") ",  "
					   JSON_PROP("n", 10) ",  "
					   JSON_PROP("wid", 1) ",  "
					   JSON_PROP("i", 1) " }");
	assert(tsobj_tp_disp_proc(node) == NULL);
	json_node_destroy(node);
}

int tsload_test_main() {
	test_tpd_bad();
	test_tpd_empty();
	test_invalid_tpd_type_type();
	test_invalid_tpd_type_value();

	test_invalid_tpd_rr_unused();
	test_tpd_rr();

	test_tpd_fillup_invalid_n_type();
	test_tpd_fillup();
	test_tpd_fillup_unused();

	return 0;
}
