/*
 * node.c
 *
 *  Created on: Apr 26, 2014
 *      Author: myaut
 */

#include <json.h>
#include <jsonimpl.h>

#include <string.h>

int64_t json_as_integer(json_node_t* node) {
	if(node->jn_type == JSON_NUMBER && node->jn_is_integer) {
		return node->jn_data.i;
	}

	json_set_error(JSON_INVALID_TYPE);
	return -1;
}

double json_as_float(json_node_t* node) {
	if(node->jn_type == JSON_NUMBER && !node->jn_is_integer) {
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
 * Get child by id.
 *
 * If id is out of bounds sets errno to JSON_NOT_FOUN
 * If parent is not JSON_NODE or JSON_ARRAY, sets errno to JSON_INVALID_TYPE
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

/* Getters with type checking
 * ------------------------- */

int64_t json_get_integer(json_node_t* parent, const char* name, int64_t def) {
	json_node_t* node = json_find(parent, name);

	if(node == NULL) {
		return def;
	}

	if(node->jn_type == JSON_NUMBER && node->jn_is_integer) {
		return node->jn_data.i;
	}

	json_set_error_str(JSON_INVALID_TYPE, "VALUE '%s' is not an integer", name);
	return def;
}

double json_get_double(json_node_t* parent, const char* name, double def) {
	json_node_t* node = json_find(parent, name);

	if(node == NULL) {
		return def;
	}

	if(node->jn_type == JSON_NUMBER && !node->jn_is_integer) {
		return node->jn_data.d;
	}

	json_set_error_str(JSON_INVALID_TYPE, "VALUE '%s' is not a floating-point value", name);
	return def;
}

const char* json_get_string(json_node_t* parent, const char* name, const char* def) {
	json_node_t* node = json_find(parent, name);

	if(node == NULL) {
		return def;
	}

	if(node->jn_type != JSON_STRING) {
		json_set_error_str(JSON_INVALID_TYPE, "VALUE '%s' is not a string", name);
		return def;
	}

	return (const char*) node->jn_data.s;
}

boolean_t json_get_boolean(json_node_t* parent, const char* name, boolean_t def) {
	json_node_t* node = json_find(parent, name);

	if(node == NULL) {
		return def;
	}

	if(node->jn_type != JSON_BOOLEAN) {
		json_set_error_str(JSON_INVALID_TYPE, "VALUE '%s' is not a boolean value", name);
		return B_FALSE;
	}

	return node->jn_data.b;
}

json_node_t* json_get_array(json_node_t* parent, const char* name) {
	json_node_t* node = json_find(parent, name);

	if(node == NULL) {
		return NULL;
	}

	if(node->jn_type != JSON_ARRAY) {
		json_set_error_str(JSON_INVALID_TYPE, "VALUE '%s' is not an array", name);
		return NULL;
	}

	return node;
}

json_node_t* json_get_node(json_node_t* parent, const char* name) {
	json_node_t* node = json_find(parent, name);

	if(node == NULL) {
		return NULL;
	}

	if(node->jn_type != JSON_NODE) {
		json_set_error_str(JSON_INVALID_TYPE, "VALUE '%s' is not an array", name);
		return NULL;
	}

	return node;
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

json_node_t* json_new_node(void) {
	return json_node_create(NULL, JSON_NODE);
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

	list_del(&node->jn_child_node);
	return JSON_OK;
}
