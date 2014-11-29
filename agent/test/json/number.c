
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

    TSLoad is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation version 3.

    TSLoad is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TSLoad.  If not, see <http://www.gnu.org/licenses/>.    
*/    



#include <tsload/defs.h>

#include <tsload/json/json.h>

#include <string.h>
#include <assert.h>


void dump_error(void);
void dump_node(json_node_t* node);

void test_number_int(void) {
	json_buffer_t* buf = JSON_BUFFER("  -123456  ");
	json_node_t* num;

	assert(json_parse(buf, &num) == JSON_OK);
	assert(json_as_integer(num) == -123456);

	json_node_destroy(num);
}

void test_number_int_inval(void) {
	json_buffer_t* buf = JSON_BUFFER("  123+456  ");
	json_node_t* num;

	assert(json_parse(buf, &num) == JSON_NUMBER_INVALID);

	dump_error();
}

void test_number_int_overflow(void) {
	json_buffer_t* buf = JSON_BUFFER("  18446744073709551617  ");
	json_node_t* num;

	assert(json_parse(buf, &num) == JSON_NUMBER_OVERFLOW);

	dump_error();
}

void test_number_float_frac(void) {
	json_buffer_t* buf = JSON_BUFFER("  -123.456  ");
	json_node_t* num;

	double val;

	assert(json_parse(buf, &num) == JSON_OK);
	val = json_as_double(num);

	assert(val > -123.4561 && val < -123.4559);

	json_node_destroy(num);
}

void test_number_float_exp(void) {
	json_buffer_t* buf = JSON_BUFFER("  -0.456E+10  ");
	json_node_t* num;

	assert(json_parse(buf, &num) == JSON_OK);
	assert(json_as_double(num) == -0.456E+10);

	json_node_destroy(num);
}

void test_number_float_inval(void) {
	json_buffer_t* buf = JSON_BUFFER("  1.e+23E45.6  ");
	json_node_t* num;

	assert(json_parse(buf, &num) == JSON_NUMBER_INVALID);

	dump_error();
}

void test_number_float_int(void) {
	json_buffer_t* buf = JSON_BUFFER("  -123  ");
	json_node_t* num;

	assert(json_parse(buf, &num) == JSON_OK);
	assert(json_as_double_n(num) == -123.0);

	json_node_destroy(num);
}

int json_test_main(void) {
	test_number_int();
	test_number_int_inval();
	test_number_int_overflow();

	test_number_float_frac();
	test_number_float_exp();
	test_number_float_inval();
	test_number_float_int();

	return 0;
}

