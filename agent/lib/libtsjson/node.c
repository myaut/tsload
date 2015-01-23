
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

#include <tsload/autostring.h>

#include <tsload/json/json.h>
#include <jsonimpl.h>

#include <string.h>
#include <limits.h>

/**
 * By default libtsjson, and top-level libraries treat unused attributes as an errors
 * On one hand it protects from protocol differences or typos, but may be painful.
 * This tuneable disables unused checks (note that if you set it to true, no warnings
 * will be displayed).
 */
boolean_t json_ignore_unused = B_FALSE;

static const char* json_type_names[] = {
	"NULL",
	"STRING",
	"NUMBER",
	"BOOLEAN",
	"ARRAY",
	"NODE",

	"N_INTEGER",
	"N_DOUBLE"
};

int json_check_type(json_node_t* node, json_type_t type) {
	boolean_t is_integer;
	boolean_t check_number;

	if(node == NULL) {
		return JSON_NOT_FOUND;
	}

	if(type == JSON_ANY) {
		return JSON_OK;
	}

	if(type == JSON_NUMBER_INTEGER) {
		is_integer = B_TRUE;
		check_number = B_TRUE;
		type = JSON_NUMBER;
	}
	else if(type == JSON_NUMBER_FLOAT) {
		is_integer = B_FALSE;
		check_number = B_TRUE;
		type = JSON_NUMBER;
	}
	else {
		is_integer = B_FALSE;
		check_number = B_FALSE;
	}

	if(unlikely(node->jn_type != type)) {
		json_set_error_str(JSON_INVALID_TYPE, "attribute '%s' have wrong type: %s (%s was expected)",
							 json_safe_name(node), json_type_names[node->jn_type], json_type_names[type]);
		return JSON_INVALID_TYPE;
	}

	if(check_number && unlikely(node->jn_is_integer != is_integer)) {
		json_set_error_str(JSON_INVALID_TYPE, "attribute '%s' have wrong number type: %s was expected",
							 json_safe_name(node), is_integer? "integer" : "float");
		return JSON_INVALID_TYPE;
	}

	return JSON_OK;
}

int json_check_unused(json_node_t* node) {
	json_node_t* child;

	if(json_ignore_unused)
		return JSON_OK;

	list_for_each_entry(json_node_t, child, &node->jn_child_head, jn_child_node) {
		if(!child->jn_touched) {
			json_set_error_str(JSON_UNUSED_CHILD, "attribute '%s' is not used", json_safe_name(child));
			return JSON_UNUSED_CHILD;
		}
	}

	return JSON_OK;
}

int64_t json_as_integer(json_node_t* node) {
	if(json_check_type(node, JSON_NUMBER_INTEGER) != JSON_OK) {
		return -1;
	}

	return node->jn_data.i;
}

double json_as_double(json_node_t* node) {
	if(json_check_type(node, JSON_NUMBER_FLOAT) != JSON_OK) {
		return 0.0;
	}

	return node->jn_data.d;
}

double json_as_double_n(json_node_t* node) {
	if(json_check_type(node, JSON_NUMBER) != JSON_OK) {
		return 0.0;
	}

	if(node->jn_is_integer)
		return (double) node->jn_data.i;

	return node->jn_data.d;
}

const char* json_as_string(json_node_t* node) {
	if(json_check_type(node, JSON_STRING) != JSON_OK) {
		return NULL;
	}

	return (const char*) node->jn_data.s;
}

boolean_t json_as_boolean(json_node_t* node) {
	if(json_check_type(node, JSON_BOOLEAN) != JSON_OK) {
		return B_FALSE;
	}

	return node->jn_data.b;
}

json_node_t* json_first(json_node_t* parent, int* id) {
	if(parent->jn_type != JSON_ARRAY && parent->jn_type != JSON_NODE) {
		json_set_error(JSON_INVALID_TYPE);
		return NULL;
	}

	*id = 0;

	if(parent->jn_children_count == 0)
		return NULL;
	return list_first_entry(json_node_t, &parent->jn_child_head, jn_child_node);
}

/**
 * Find child by name.
 *
 * If node is not found, returns NULL, if parent is not JSON_NODE, sets JSON_INVALID_TYPE
 */
json_node_t* json_find_opt(json_node_t* parent, const char* name) {
	json_node_t* node;

	if(parent->jn_type != JSON_NODE) {
		json_set_error(JSON_INVALID_TYPE);
		return NULL;
	}

	/* TODO: Implement dynamic hash_map for parents with large
	 * number of children */

	list_for_each_entry(json_node_t, node, &parent->jn_child_head, jn_child_node) {
		if(strcmp(json_name(node), name) == 0) {
			node->jn_touched = B_TRUE;
			return node;
		}
	}

	return NULL;
}

/**
 * Find child by name.
 *
 * If node is not found, sets errno to JSON_NOT_FOUND.
 * If parent is not JSON_NODE, sets errno to JSON_INVALID_TYPE
 */
json_node_t* json_find(json_node_t* parent, const char* name) {
	json_node_t* node = json_find_opt(parent, name);

	if(node == NULL) {
		json_set_error_str(JSON_NOT_FOUND, "attribute '%s' not found", name);
	}

	return node;
}

/**
 * Get child by id.
 *
 * If id is out of bounds sets errno to JSON_NOT_FOUND
 * If parent is not JSON_NODE or JSON_ARRAY, sets errno to JSON_INVALID_TYPE
 *
 * @see json_popitem
 */
json_node_t* json_getitem(json_node_t* parent, int id) {
	json_node_t* node;
	int nid = 0;

	if(parent->jn_type != JSON_ARRAY && parent->jn_type != JSON_NODE) {
		json_set_error(JSON_INVALID_TYPE);
		return NULL;
	}

	/* TODO: Implement array for parents with large number of children */

	if(id >= parent->jn_children_count || id < 0) {
		json_set_error_str(JSON_NOT_FOUND, "attribute #%d is out of bounds", id);
	}

	list_for_each_entry(json_node_t, node, &parent->jn_child_head, jn_child_node) {
		if(nid == id)
			return node;

		++nid;
	}

	/* jn_children_count is out of sync?? */
	json_set_error(JSON_INTERNAL_ERROR);
	return NULL;
}

/**
 * Gets child by id and extracts it from node
 *
 * @see json_getitem
 */
json_node_t* json_popitem(json_node_t* parent, int id) {
	json_node_t* node = json_getitem(parent, id);

	if(node != NULL) {
		list_del(&node->jn_child_node);
	}

	return node;
}

/* Getters with type checking
 * ------------------------- */

#define DECLARE_JSON_GET_INTEGER(suffix, type, min, max)							\
	int json_get_integer_ ## suffix(json_node_t* parent, 							\
				const char* name, type* val) {										\
		json_node_t* node = json_find(parent, name);								\
		int err = json_check_type(node, JSON_NUMBER_INTEGER);						\
		int64_t i;																	\
		if(err != JSON_OK)															\
			return err;																\
		i = node->jn_data.i;														\
		if(i < (min) || i > (max)) {												\
			json_set_error_str(JSON_OVERFLOW, "attribute '%s' integer overflow: "	\
							   "%" PRId64 " not fits into type %s", name, i, #type);\
			return JSON_OVERFLOW;													\
		}																			\
		*val = (type) i;															\
		return JSON_OK;																\
	}

int json_get_integer_i64(json_node_t* parent, const char* name, int64_t* val) {
	json_node_t* node = json_find(parent, name);
	int err = json_check_type(node, JSON_NUMBER_INTEGER);

	if(err != JSON_OK)
		return err;

	*val = node->jn_data.i;
	return JSON_OK;
}

DECLARE_JSON_GET_INTEGER(u8, uint8_t, 0, 255)
DECLARE_JSON_GET_INTEGER(u16, uint16_t, 0, 65535)
DECLARE_JSON_GET_INTEGER(u32, uint32_t, 0, 4294967295)

DECLARE_JSON_GET_INTEGER(i8, int8_t, -128, 127)
DECLARE_JSON_GET_INTEGER(i16, int16_t, -32768, 32767)
DECLARE_JSON_GET_INTEGER(i32, int32_t,	-2147483648, 2147483647)

DECLARE_JSON_GET_INTEGER(i, int, INT_MIN, INT_MAX)
DECLARE_JSON_GET_INTEGER(l, long, LONG_MIN, LONG_MAX)
DECLARE_JSON_GET_INTEGER(ll, long long, LLONG_MIN, LLONG_MAX)

DECLARE_JSON_GET_INTEGER(u, unsigned, 0, UINT_MAX)
DECLARE_JSON_GET_INTEGER(ul, unsigned long, 0, ULONG_MAX)

DECLARE_JSON_GET_INTEGER(tm, ts_time_t, 0, TS_TIME_MAX)

int json_get_double(json_node_t* parent, const char* name, double* val) {
	json_node_t* node = json_find(parent, name);
	int err = json_check_type(node, JSON_NUMBER_FLOAT);

	if(err != JSON_OK)
		return err;

	*val = node->jn_data.d;
	return JSON_OK;
}

int json_get_double_n(json_node_t* parent, const char* name, double* val) {
	json_node_t* node = json_find(parent, name);
	int err = json_check_type(node, JSON_NUMBER);

	if(err != JSON_OK)
		return err;

	if(node->jn_is_integer) {
		*val = (double) node->jn_data.i;
	}
	else {
		*val = node->jn_data.d;
	}

	return JSON_OK;
}

int json_get_string(json_node_t* parent, const char* name, char** val) {
	json_node_t* node = json_find(parent, name);
	int err = json_check_type(node, JSON_STRING);

	if(err != JSON_OK)
		return err;

	*val = (char*) node->jn_data.s;
	return JSON_OK;
}

int json_get_string_copy(json_node_t* parent, const char* name, char* val, size_t len) {
	json_node_t* node = json_find(parent, name);
	int err = json_check_type(node, JSON_STRING);

	if(err != JSON_OK)
		return err;
	if(len < strlen((char*) node->jn_data.s)) {
		json_set_error_str(JSON_OVERFLOW, "attribute '%s' string too long: doesn't "
						   "fit into buffer of %d chars", name, len);
		return JSON_OVERFLOW;
	}

	strncpy(val, (char*) node->jn_data.s, len);
	return JSON_OK;
}

int json_get_string_aas(json_node_t* parent, const char* name, char** aas) {
	char* str;
	size_t aas_ret;
	int ret = json_get_string(parent, name, &str);

	if(ret != JSON_OK)
		return ret;

	aas_ret = aas_copy(aas, str);

	if(!AAS_IS_OK(aas_ret)) {
		json_set_error_str(JSON_INTERNAL_ERROR, "AAS error: %x", aas_ret);
		return JSON_INTERNAL_ERROR;
	}

	return JSON_OK;
}

int json_get_boolean(json_node_t* parent, const char* name, boolean_t* val) {
	json_node_t* node = json_find(parent, name);
	int err = json_check_type(node, JSON_BOOLEAN);

	if(err != JSON_OK)
		return err;

	*val = node->jn_data.b;
	return JSON_OK;
}

int json_get_array(json_node_t* parent, const char* name, json_node_t** val) {
	json_node_t* node = json_find(parent, name);
	int err = json_check_type(node, JSON_ARRAY);

	if(err != JSON_OK)
		return err;

	*val = node;
	return JSON_OK;
}

int json_get_node(json_node_t* parent, const char* name, json_node_t** val) {
	json_node_t* node = json_find(parent, name);
	int err = json_check_type(node, JSON_NODE);

	if(err != JSON_OK)
		return err;

	*val = node;
	return JSON_OK;
}

/* Node constructors
 * ------------------------- */
json_node_t* json_new_integer(int64_t val) {
	json_node_t* node = json_node_create(NULL, JSON_NUMBER);

	if(!node)
		return NULL;

	node->jn_is_integer = B_TRUE;
	node->jn_data.i = val;

	return node;
}

json_node_t* json_new_double(double val) {
	json_node_t* node = json_node_create(NULL, JSON_NUMBER);

	if(!node)
		return NULL;

	node->jn_is_integer = B_FALSE;
	node->jn_data.d = val;

	return node;
}

/**
 * Create new string JSON node.
 *
 * @param val pointer to "JSON string". Create it statically via JSON_STR() macro or \
 * 			dynamically using json_str_create()
 */
json_node_t* json_new_string(json_str_t val) {
	json_node_t* node = json_node_create(NULL, JSON_STRING);

	if(!node)
		return NULL;

	node->jn_data.s = val;

	return node;
}

json_node_t* json_new_boolean(boolean_t val) {
	json_node_t* node = json_node_create(NULL, JSON_BOOLEAN);

	if(!node)
		return NULL;

	node->jn_data.b = val;

	return node;
}

json_node_t* json_new_array(void) {
	return json_node_create(NULL, JSON_ARRAY);
}

json_node_t* json_new_node(const char* node_class) {
	json_node_t* node = json_node_create(NULL, JSON_NODE);

	if(node_class != NULL) {
		json_add_string(node, JSON_STR("_type"),
				json_str_create(node_class));
	}

	return node;
}

void json_set_integer(json_node_t* node, int64_t val) {
	if(node->jn_type == JSON_NUMBER && node->jn_is_integer) {
		node->jn_data.i = val;
	}

	json_set_error(JSON_INVALID_TYPE);
}

void json_set_double(json_node_t* node, double val) {
	if(node->jn_type == JSON_NUMBER && !node->jn_is_integer) {
		node->jn_data.d = val;
	}
}

void json_set_string(json_node_t* node, json_str_t val) {
	if(node->jn_type == JSON_STRING) {
		if(node->jn_data.s != NULL) {
			json_str_free(node->jn_data.s, node->jn_buf);
		}

		node->jn_data.s = val;
	}

	json_set_error(JSON_INVALID_TYPE);
}

int json_set_number(json_node_t* node, const char* val) {
	struct json_parser parser;
	json_buffer_t buf;

	parser.index = 0;
	parser.lineno = 0;
	parser.newline = 0;

	buf.buffer = val;
	buf.size = strlen(val);

	return json_parse_number(&parser, &buf, &node);
}

void json_set_boolean(json_node_t* node, boolean_t val) {
	if(node->jn_type == JSON_BOOLEAN) {
		node->jn_data.b = val;
	}

	json_set_error(JSON_INVALID_TYPE);
}

int json_add_node(json_node_t* parent, json_str_t name, json_node_t* node) {
	node->jn_parent = parent;
	node->jn_name = name;

	list_add_tail(&node->jn_child_node, &parent->jn_child_head);
	++parent->jn_children_count;

	return JSON_OK;
}

int json_remove_node(json_node_t* parent, json_node_t* node) {
	if(node->jn_parent != parent)
		return json_set_error(JSON_NOT_CHILD);

	--parent->jn_children_count;
	list_del(&node->jn_child_node);
	return JSON_OK;
}

json_node_t* json_copy_node(json_node_t* node) {
	json_node_t* copy = json_node_create_copy(node);
	json_node_t* child;
	json_node_t* child_copy;

	list_for_each_entry(json_node_t, child, &node->jn_child_head, jn_child_node) {
		child_copy = json_copy_node(child);
		child_copy->jn_parent = copy;

		list_add_tail(&child_copy->jn_child_node, &copy->jn_child_head);
		++copy->jn_children_count;
	}

	return copy;
}
