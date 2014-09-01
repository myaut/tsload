/*
 * node.c
 *
 *  Created on: Apr 26, 2014
 *      Author: myaut
 */

#include <json.h>
#include <jsonimpl.h>

#include <string.h>

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

int64_t json_as_integer(json_node_t* node) {
	if(node->jn_type == JSON_NUMBER && node->jn_is_integer) {
		return node->jn_data.i;
	}

	json_set_error(JSON_INVALID_TYPE);
	return -1;
}

double json_as_double(json_node_t* node) {
	if(node->jn_type == JSON_NUMBER && !node->jn_is_integer) {
		return node->jn_data.d;
	}

	json_set_error(JSON_INVALID_TYPE);
	return 0.0;
}

double json_as_double_n(json_node_t* node) {
	if(node->jn_type == JSON_NUMBER) {
		if(node->jn_is_integer)
			return (double) node->jn_data.i;

		return node->jn_data.d;
	}

	json_set_error(JSON_INVALID_TYPE);
	return 0.0;
}

const char* json_as_string(json_node_t* node) {
	if(node->jn_type != JSON_STRING) {
		json_set_error(JSON_INVALID_TYPE);
		return NULL;
	}

	return (const char*) node->jn_data.s;
}

boolean_t json_as_boolean(json_node_t* node) {
	if(node->jn_type != JSON_BOOLEAN) {
		json_set_error(JSON_INVALID_TYPE);
		return B_FALSE;
	}

	return node->jn_data.b;
}

json_node_t* json_first(json_node_t* parent, int* id) {
	if(parent->jn_type != JSON_ARRAY && parent->jn_type != JSON_NODE) {
		json_set_error(JSON_INVALID_TYPE);
		return NULL;
	}

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
		json_set_error_str(JSON_NOT_FOUND, "VALUE '%s' not found", name);
	}

	return node;
}

/**
 * Find child by name and check its type
 *
 * If node is not found, sets errno to JSON_NOT_FOUND.
 * If node type is not match type, sets errno to JSON_INVALID_TYPE
 * If parent is not JSON_NODE, sets errno to JSON_INVALID_TYPE
 */
json_node_t* json_find_bytype(json_node_t* parent, const char* name, json_type_t type) {
	json_node_t* node = json_find(parent, name);

	if(type == JSON_ANY) {
		return node;
	}

	if(node != NULL) {
		boolean_t is_integer;
		boolean_t check_number;

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
			json_set_error_str(JSON_INVALID_TYPE, "VALUE '%s' have wrong type: %s (%s was expected)",
							   	   	   name, json_type_names[node->jn_type], json_type_names[type]);
			return NULL;
		}

		if(check_number && unlikely(node->jn_is_integer != is_integer)) {
			json_set_error_str(JSON_INVALID_TYPE, "VALUE '%s' have wrong number type: %s was expected",
										name, is_integer? "integer" : "float");
			return NULL;
		}
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
		json_set_error_str(JSON_NOT_FOUND, "VALUE #%d is out of bounds", id);
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

int json_get_integer_i64(json_node_t* parent, const char* name, int64_t* val) {
	json_node_t* node = json_find_bytype(parent, name, JSON_NUMBER_INTEGER);

	if(node == NULL) {
		return json_errno();
	}

	*val = node->jn_data.i;
	return JSON_OK;
}

int json_get_double(json_node_t* parent, const char* name, double* val) {
	json_node_t* node = json_find_bytype(parent, name, JSON_NUMBER_FLOAT);

	if(node == NULL) {
		return json_errno();
	}

	*val = node->jn_data.d;
	return JSON_OK;
}

int json_get_double_n(json_node_t* parent, const char* name, double* val) {
	json_node_t* node = json_find_bytype(parent, name, JSON_NUMBER);

	if(node == NULL) {
		return json_errno();
	}

	if(node->jn_is_integer) {
		*val = (double) node->jn_data.i;
	}
	else {
		*val = node->jn_data.d;
	}

	return JSON_OK;
}

int json_get_string(json_node_t* parent, const char* name, char** val) {
	json_node_t* node = json_find_bytype(parent, name, JSON_STRING);

	if(node == NULL) {
		return json_errno();
	}

	*val = (char*) node->jn_data.s;
	return JSON_OK;
}

int json_get_boolean(json_node_t* parent, const char* name, boolean_t* val) {
	json_node_t* node = json_find_bytype(parent, name, JSON_BOOLEAN);

	if(node == NULL) {
		return json_errno();
	}

	*val = node->jn_data.b;
	return JSON_OK;
}

int json_get_array(json_node_t* parent, const char* name, json_node_t** val) {
	json_node_t* node = json_find_bytype(parent, name, JSON_NODE);

	if(node == NULL) {
		return json_errno();
	}

	*val = node;
	return JSON_OK;
}

int json_get_node(json_node_t* parent, const char* name, json_node_t** val) {
	json_node_t* node = json_find_bytype(parent, name, JSON_NODE);

	if(node == NULL) {
		return json_errno();
	}

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
	return json_node_create(NULL, JSON_NODE);
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
			json_str_free(node->jn_data.s, NULL);
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

	return json_parse_number(&parser, &buf, val);
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
