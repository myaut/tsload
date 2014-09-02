/*
 * randgen.c
 *
 *  Created on: Sep 1, 2014
 *      Author: myaut
 */

#define NO_JSON

#include <tsobj.h>
#include <randgen.h>

#include "helpers.h"

#include <stdio.h>

#include <assert.h>

randgen_t* generator = NULL;

void test_rg_bad(void) {
	TEST_PREAMBLE(" [] ");
	assert(tsobj_randgen_proc(node) == NULL);
}

void test_rg_empty(void) {
	TEST_PREAMBLE("{  }");
	assert(tsobj_randgen_proc(node) == NULL);
}

void test_invalid_rg_class_type(void) {
	TEST_PREAMBLE("{ " JSON_PROP("class", 1.1) " }");
	assert(tsobj_randgen_proc(node) == NULL);
}

void test_invalid_rg_class_value(void) {
	TEST_PREAMBLE("{ " JSON_PROP("class", "rg_invalid_class") " }");
	assert(tsobj_randgen_proc(node) == NULL);
}

void test_valid_rg_class(void) {
	randgen_t* rg;
	TEST_PREAMBLE("{ " JSON_PROP("class", "lcg") " }");

	assert((rg = tsobj_randgen_proc(node)) != NULL);

	rg_destroy(rg);
}

void test_invalid_seed_type(void) {
	TEST_PREAMBLE("{ " JSON_PROP("class", "lcg") ", "
					   JSON_PROP("seed", 1e200) " }");
	assert(tsobj_randgen_proc(node) == NULL);
}

void test_valid_seed(void) {
	randgen_t* rg;
	TEST_PREAMBLE("{ " JSON_PROP("class", "lcg") ", "
			   	   	   JSON_PROP("seed", 1000) " }");

	assert((rg = tsobj_randgen_proc(node)) != NULL);
	assert(rg->rg_seed == 1000);

	rg_destroy(rg);
}

void test_invalid_rv_class_type(void) {
	TEST_PREAMBLE("{ " JSON_PROP2("class", "[]") " }");
	assert(tsobj_randvar_proc(node, generator) == NULL);
}

void test_invalid_rv_class_value(void) {
	TEST_PREAMBLE("{ " JSON_PROP("class", "rv_invalid_class") " }");
	assert(tsobj_randvar_proc(node, generator) == NULL);
}

void test_invalid_rv_exponential_rate_type(void) {
	TEST_PREAMBLE("{ " JSON_PROP("class", "exponential") ", "
					   JSON_PROP("rate", "rate_value")" }");
	assert(tsobj_randvar_proc(node, generator) == NULL);
}

void test_invalid_rv_exponential_rate_value(void) {
	TEST_PREAMBLE("{ " JSON_PROP("class", "exponential") ", "
					   JSON_PROP("rate", -10.0)" }");
	assert(tsobj_randvar_proc(node, generator) == NULL);
}

void test_valid_rv_exponential_rate_int(void) {
	randvar_t* rv;

	TEST_PREAMBLE("{ " JSON_PROP("class", "exponential") ", "
					   JSON_PROP("rate", 2)" }");
	assert((rv = tsobj_randvar_proc(node, generator)) != NULL);

	rv_destroy(rv);
}

void test_valid_rv_exponential_rate_double(void) {
	randvar_t* rv;

	TEST_PREAMBLE("{ " JSON_PROP("class", "exponential") ", "
					   JSON_PROP("rate", 2.0)" }");
	assert((rv = tsobj_randvar_proc(node, generator)) != NULL);

	rv_destroy(rv);
}

int tsload_test_main() {
	test_rg_bad();
	test_rg_empty();

	test_invalid_rg_class_type();
	test_invalid_rg_class_value();
	test_valid_rg_class();
	test_invalid_seed_type();
	test_valid_seed();

	generator = rg_create(&rg_lcg_class, 0);

	test_invalid_rv_class_type();
	test_invalid_rv_class_value();
	test_invalid_rv_exponential_rate_type();
	test_invalid_rv_exponential_rate_value();
	test_valid_rv_exponential_rate_int();
	test_valid_rv_exponential_rate_double();

	rg_destroy(generator);

	return 0;
}
