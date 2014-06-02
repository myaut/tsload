
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



#include <json.h>

#include <string.h>
#include <assert.h>

void dump_error(void);
void dump_node(json_node_t* node);

void test_simple_string(void) {
	json_buffer_t* buf = JSON_BUFFER("  \"simple string\"  ");
	json_node_t* str;

	assert(json_parse(buf, &str) == JSON_OK);

	dump_node(str);
	assert(strcmp(json_as_string(str), "simple string") == 0);

	json_node_destroy(str);
}

void test_escapes(void) {
	json_buffer_t* buf = JSON_BUFFER("  \"\\n \\\" \\u044f \\t \\u044f\"  ");
	json_node_t* str;

	assert(json_parse(buf, &str) == JSON_OK);

	dump_node(str);
	assert(strcmp(json_as_string(str), "\n \" \u044f \t \u044f") == 0);

	json_node_destroy(str);
}

void test_unfinished_string(void) {
	json_buffer_t* buf = JSON_BUFFER("  \" unfinished string \\\"  ");
	json_node_t* str;

	assert(json_parse(buf, &str) == JSON_END_OF_BUFFER);

	dump_error();
}

void test_unescaped_string(void) {
	json_buffer_t* buf = JSON_BUFFER("  \" unescaped \n string \"  ");
	json_node_t* str;

	assert(json_parse(buf, &str) == JSON_STRING_UNESCAPED);

	dump_error();
}

void test_invalid_escape(void) {
	json_buffer_t* buf = JSON_BUFFER("  \" \\x55 \"  ");
	json_node_t* str;

	assert(json_parse(buf, &str) == JSON_STRING_INVALID_ESC);

	dump_error();
}

void test_invalid_unicode_escape_1(void) {
	json_buffer_t* buf = JSON_BUFFER("  \" \\uaaxy\"  ");
	json_node_t* str;

	assert(json_parse(buf, &str) == JSON_STRING_INVALID_ESC);

	dump_error();
}

void test_invalid_unicode_escape_2(void) {
	json_buffer_t* buf = JSON_BUFFER("  \" \\uaa\"  ");
	json_node_t* str;

	assert(json_parse(buf, &str) == JSON_END_OF_BUFFER);

	dump_error();
}


int json_test_main(void) {
	test_simple_string();
	test_escapes();

	test_unfinished_string();
	test_unescaped_string();
	test_invalid_escape();
	test_invalid_unicode_escape_1();
	test_invalid_unicode_escape_2();

	return 0;
}

