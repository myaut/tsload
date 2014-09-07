
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

#include <stdio.h>
#include <string.h>
#include <assert.h>

#define WRITER_SIMPLE_RESULT		"{\"i\":12790}"
void test_writer_simple(void) {
	char buf[32] = {'a'};
	json_node_t* node = json_new_node(NULL);

	json_add_node(node, JSON_STR("i"), json_new_integer(12790));

	assert(json_write_count(node, B_FALSE) == sizeof(WRITER_SIMPLE_RESULT));
	assert(json_write_buf(node, buf, 32, B_FALSE) == JSON_OK);

	puts(buf);

	assert(strcmp(buf, WRITER_SIMPLE_RESULT) == 0);

	json_node_destroy(node);
}

#define WRITER_FORMATTED		"{\n  \"i\": 65536,\n  \"xx\": \"\\t\"\n}\n"
void test_writer_formatted(void) {
	char buf[40] = {'a'};
	json_node_t* node = json_new_node(NULL);

	json_add_node(node, JSON_STR("i"), json_new_integer(65536));
	json_add_node(node, JSON_STR("xx"), json_new_string(JSON_STR("\t")));

	assert(json_write_count(node, B_TRUE) == sizeof(WRITER_FORMATTED));
	assert(json_write_buf(node, buf, 40, B_TRUE) == JSON_OK);

	puts(buf);

	assert(strcmp(buf, WRITER_FORMATTED) == 0);

	json_node_destroy(node);
}

int json_test_main(void) {
	test_writer_simple();
	test_writer_formatted();

	return 0;
}

