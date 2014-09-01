/*
 * number.c
 *
 *  Created on: Apr 26, 2014
 *      Author: myaut
 */

#include <json.h>

#include <string.h>
#include <assert.h>

void dump_error(void);
void dump_node(json_node_t* node);

void test_object_empty(void) {
	json_buffer_t* buf = JSON_BUFFER(" [] ");
	json_node_t* obj;

	assert(json_parse(buf, &obj) == JSON_OK);
	assert(json_size(obj) == 0);

	json_node_destroy(obj);
}

void test_object_one(void) {
	json_buffer_t* buf = JSON_BUFFER(" [true] ");
	json_node_t* obj;
	json_node_t* el1;

	assert(json_parse(buf, &obj) == JSON_OK);
	assert(json_size(obj) == 1);

	json_node_destroy(obj);
}

void test_object_many(void) {
	json_buffer_t* buf = JSON_BUFFER(" [true,  10,\"hi!\"] ");
	json_node_t* obj;
	json_node_t* el;

	assert(json_parse(buf, &obj) == JSON_OK);
	assert(json_size(obj) == 3);

	el = json_getitem(obj, 0);
	assert(el != NULL);
	assert(json_as_boolean(el) == B_TRUE);

	el = json_getitem(obj, 1);
	assert(el != NULL);
	assert(json_as_integer(el) == 10);

	el = json_getitem(obj, 2);
	assert(el != NULL);
	assert(strcmp(json_as_string(el), "hi!") == 0);

	json_node_destroy(obj);
}

void test_object_node(void) {
	json_buffer_t* buf = JSON_BUFFER(" { \"b\":true,  \"_int\":  10,   \"str\":\"hi!\" } ");
	json_node_t* obj;

	boolean_t b = B_FALSE;
	int i = -1;
	char* s = "bye!";

	assert(json_parse(buf, &obj) == JSON_OK);
	assert(json_size(obj) == 3);

	assert(json_get_boolean(obj, "b", &b) == JSON_OK);
	assert(b == B_TRUE);

	assert(json_get_integer_i(obj, "_int", &i) == JSON_OK);
	assert(i == 10);

	assert(json_get_string(obj, "str", &s) == JSON_OK);
	assert(strcmp(s, "hi!") == 0);

	json_node_destroy(obj);
}

void test_object_embed(void) {
	json_buffer_t* buf = JSON_BUFFER(" { \"a\":{},  \"b\":[] } ");
	json_node_t* obj;

	assert(json_parse(buf, &obj) == JSON_OK);
	assert(json_size(obj) == 2);

	json_node_destroy(obj);
}

void test_object_missing_comma(void) {
	json_buffer_t* buf = JSON_BUFFER(" { \"a\":null  \"b\":null } ");
	json_node_t* obj;

	assert(json_parse(buf, &obj) == JSON_OBJECT_INVALID);

	dump_error();
}

void test_object_wrong_bracket(void) {
	json_buffer_t* buf = JSON_BUFFER(" { \"a\":null, \"b\":null ] ");
	json_node_t* obj;

	assert(json_parse(buf, &obj) == JSON_OBJECT_INVALID);

	dump_error();
}

void test_object_missing_colon(void) {
	json_buffer_t* buf = JSON_BUFFER(" { \"a\"   {}, \"b\":null } ");
	json_node_t* obj;

	assert(json_parse(buf, &obj) == JSON_OBJECT_INVALID);

	dump_error();
}

void test_object_not_an_array(void) {
	json_buffer_t* buf = JSON_BUFFER(" [ \"a\": null, \"b\":null ] ");
	json_node_t* obj;

	assert(json_parse(buf, &obj) == JSON_OBJECT_INVALID);

	dump_error();
}

int json_test_main(void) {
	test_object_empty();
	test_object_one();
	test_object_many();
	test_object_embed();

	test_object_missing_comma();
	test_object_wrong_bracket();
	test_object_missing_colon();
	test_object_not_an_array();

	return 0;
}
