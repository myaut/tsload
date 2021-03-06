
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



#ifndef JSON_H_
#define JSON_H_

#include <tsload/defs.h>

#include <tsload/list.h>
#include <tsload/atomic.h>
#include <tsload/time.h>
#include <tsload/hashmap.h>


#define JSONERRLEN			256

/**
 * #### JSON strings
 *
 * Because string allocation is expensive, libtsjson references already allocated
 * strings instead of cloning them. Each JSON string starts from "tag" character
 * which defines how it was allocated:
 * 		* 'D' or JSON_STR_DYNAMIC - dynamically allocated from heap.
 * 		* 'C' or JSON_STR_CONST - immutable string in compile-time or run-time.
 * 		  Usually used for JSON node names for created nodes with JSON_STR macro.
 * 		* 'R' or JSON_STR_REFERENCE - references to parsed JSON data. Useful for node names
 * 		  as well as string data inside nodes.
 */

#define JSON_STR_DYNAMIC	 'D'
#define JSON_STR_CONST		 'C'
#define JSON_STR_REFERENCE	 'R'

#define JSON_STR(str)	 (json_str_t) ((const char*)("C" str) + 1)
#define JSON_C_STR(json_str) ((const char*) (json_str))
#define JSON_NULL_STR	 ((json_str_t) NULL)

// typedef const char* json_str_t;

typedef struct {
	char c;
} json_chr_t;
typedef const json_chr_t* json_str_t;

typedef struct json_buffer {
	char* 		buffer;
	size_t 		size;

	atomic_t    ref_count;
} json_buffer_t;

typedef enum {
	/* Type hints */
	JSON_ANY = -1,

	JSON_NUMBER_INTEGER = -20,
	JSON_NUMBER_FLOAT = -21,

	/* Node type */
    JSON_NULL = 0,
    JSON_STRING,
    JSON_NUMBER,
    JSON_BOOLEAN,
    JSON_ARRAY,
    JSON_NODE
} json_type_t;

/**
 * JSON node
 *
 * @member jn_parent parent node
 * @member jn_buf buffer that strings (jn_name or string value) that node references
 * @member jn_name name of node (JSON string). Will be set to NULL for array nodes and root node
 * @member jn_type type of node
 * @member jn_is_integer special flag that distinguishes floating point numbers from integers \
 * 						 type is decided during parsing (or json_set_number)
 * @member jn_children_count cached number of children
 * @member jn_child_head list of children
 * @member jn_touched that node was touched by json_get_* or json_find* operations. \
 * 						 Helps finding unused optional attributes (i.e. due to typo)
 */
typedef struct json_node {
	struct json_node* jn_parent;

	json_buffer_t* jn_buf;

	json_str_t	jn_name;

	json_type_t jn_type;
	boolean_t	jn_is_integer;

	unsigned    jn_children_count;
	list_head_t	jn_child_head;
	list_node_t	jn_child_node;

	boolean_t 	jn_touched;

	union {
		int64_t		i;
		double		d;
		json_str_t  s;
		boolean_t	b;
	} jn_data;
} json_node_t;

/**
 * JSON error codes
 */
#define JSON_OK					0
#define JSON_INTERNAL_ERROR		-1
#define JSON_INVALID_TYPE		-2
#define JSON_NOT_FOUND			-3
#define JSON_NOT_CHILD			-4
#define JSON_UNUSED_CHILD		-5
#define JSON_OVERFLOW			-6

/* Generic constant */
#define JSON_MIN_GENERIC_ERROR	-7

/* Parser errors */
#define JSON_END_OF_BUFFER			-11
#define JSON_VALUE_INVALID			-12
#define JSON_NUMBER_INVALID			-13
#define JSON_NUMBER_OVERFLOW		-14
#define JSON_LITERAL_INVALID		-15
#define JSON_STRING_UNESCAPED		-16
#define JSON_STRING_INVALID_ESC		-17
#define JSON_OBJECT_INVALID			-18

/* Writer common errors */
#define JSON_INVALID_UNICODE_CHR	-31
#define JSON_BUFFER_OVERFLOW		-32
#define JSON_FILE_ERROR				-33

/**
 * Hook for TSObj - catches JSON errors and passes them to TSObj
 * (note: parser errors are ignored)
 */
typedef int (*json_error_msg_func)(int error, const char* format, va_list va);
LIBEXPORT void json_register_error_msg_func(json_error_msg_func func);

typedef struct json_error_state {
	int   je_error;
	char  je_message[JSONERRLEN];

	int   je_line;
	size_t je_char;
} json_error_state_t;

/**
 * Macro from compile-time buffer creation. Used by tests mostly.
 */
#define JSON_BUFFER(str)	json_buf_create(str, sizeof(str), B_FALSE)

LIBEXPORT int json_check_type(json_node_t* node, json_type_t type);
LIBEXPORT int json_check_unused(json_node_t* node);

LIBEXPORT json_str_t json_str_create(const char* str);
LIBEXPORT json_buffer_t* json_buf_from_file(const char* path);
LIBEXPORT json_buffer_t* json_buf_create(char* data, size_t sz, boolean_t reuse);

LIBEXPORT int json_errno(void);
LIBEXPORT void json_errno_clear(void);
LIBEXPORT const char* json_error_message();
LIBEXPORT json_error_state_t* json_get_error(void);

/**
 * JSON data extractors
 * */
LIBEXPORT const char* json_as_string(json_node_t* node);
LIBEXPORT boolean_t json_as_boolean(json_node_t* node);
LIBEXPORT int64_t json_as_integer(json_node_t* node);
LIBEXPORT double json_as_double(json_node_t* node);
LIBEXPORT double json_as_double_n(json_node_t* node);

LIBEXPORT json_node_t* json_first(json_node_t* parent, int* id);
STATIC_INLINE boolean_t json_is_end(json_node_t* parent, json_node_t* node, int* id) {
	return node == NULL ||
				list_is_head(&node->jn_child_node, &parent->jn_child_head);
}
STATIC_INLINE json_node_t* json_next(json_node_t* node, int* id) {
	++(*id);
	return list_next_entry(json_node_t, node, jn_child_node);
}

/**
 * Iterate over children of JSON node
 * If parent is not JSON_NODE or JSON_ARRAY sets invalid type and
 * walks nothing
 *
 * @param parent	parent node
 * @param node		iterator
 * @param id		int variable that contains index of child
 *
 * @note id is used for compability with tsobj (Python lists precisely)
 */
#define json_for_each(parent, node, id) 			\
			for(node = json_first(parent, &id);		\
				!json_is_end(parent, node, &id);	\
				node = json_next(node, &id))

/**
 * Checks if JSON node is empty. Doesn't do any of typechecking.
 */
STATIC_INLINE boolean_t json_is_empty(json_node_t* node) {
	return node->jn_children_count == 0;
}

STATIC_INLINE size_t json_size(json_node_t* node) {
	return node->jn_children_count;
}

STATIC_INLINE const char* json_name(json_node_t* node) {
	return (const char*) node->jn_name;
}

STATIC_INLINE const char* json_safe_name(json_node_t* node) {
	return (node->jn_name == NULL) ? "(root)" : ((const char*) node->jn_name);
}

STATIC_INLINE json_type_t json_type(json_node_t* node) {
	return node->jn_type;
}

STATIC_INLINE json_type_t json_type_hinted(json_node_t* node) {
	if(node->jn_type == JSON_NUMBER) {
		if(node->jn_is_integer)
			return JSON_NUMBER_INTEGER;
		else
			return JSON_NUMBER_FLOAT;
	}

	return node->jn_type;
}

LIBEXPORT json_node_t* json_find_opt(json_node_t* parent, const char* name);
LIBEXPORT json_node_t* json_find(json_node_t* parent, const char* name);
LIBEXPORT json_node_t* json_getitem(json_node_t* parent, unsigned int id);
LIBEXPORT json_node_t* json_popitem(json_node_t* parent, unsigned int id);

LIBEXPORT int json_get_integer_i64(json_node_t* parent, const char* name, int64_t* val);
LIBEXPORT int json_get_integer_u8(json_node_t* parent, const char* name, uint8_t* val);
LIBEXPORT int json_get_integer_u16(json_node_t* parent, const char* name, uint16_t* val);
LIBEXPORT int json_get_integer_u32(json_node_t* parent, const char* name, uint32_t* val);
LIBEXPORT int json_get_integer_i8(json_node_t* parent, const char* name, int8_t* val);
LIBEXPORT int json_get_integer_i16(json_node_t* parent, const char* name, int16_t* val);
LIBEXPORT int json_get_integer_i32(json_node_t* parent, const char* name, int32_t* val);
LIBEXPORT int json_get_integer_i(json_node_t* parent, const char* name, int* val);
LIBEXPORT int json_get_integer_l(json_node_t* parent, const char* name, long* val);
LIBEXPORT int json_get_integer_ll(json_node_t* parent, const char* name, long long* val);
LIBEXPORT int json_get_integer_u(json_node_t* parent, const char* name, unsigned* val);
LIBEXPORT int json_get_integer_ul(json_node_t* parent, const char* name, unsigned long* val);
LIBEXPORT int json_get_integer_tm(json_node_t* parent, const char* name, ts_time_t* val);
LIBEXPORT int json_get_double(json_node_t* parent, const char* name, double* val);
LIBEXPORT int json_get_double_n(json_node_t* parent, const char* name, double* val);
LIBEXPORT int json_get_string(json_node_t* parent, const char* name, const char** val);
LIBEXPORT int json_get_boolean(json_node_t* parent, const char* name, boolean_t* val);
LIBEXPORT int json_get_array(json_node_t* parent, const char* name, json_node_t** val);
LIBEXPORT int json_get_node(json_node_t* parent, const char* name, json_node_t** val);

LIBEXPORT int json_get_string_copy(json_node_t* parent, const char* name, char* val, size_t len);
LIBEXPORT int json_get_string_aas(json_node_t* parent, const char* name, char** aas);

LIBEXPORT json_node_t* json_new_null(void);
LIBEXPORT json_node_t* json_new_integer(int64_t val);
LIBEXPORT json_node_t* json_new_double(double val);
LIBEXPORT json_node_t* json_new_string(json_str_t val);
LIBEXPORT json_node_t* json_new_boolean(boolean_t val);
LIBEXPORT json_node_t* json_new_array(void);
LIBEXPORT json_node_t* json_new_node(const char* node_class);

LIBEXPORT void json_set_integer(json_node_t* node, int64_t val);
LIBEXPORT void json_set_double(json_node_t* node, double val);
LIBEXPORT void json_set_string(json_node_t* node, json_str_t val);
LIBEXPORT void json_set_boolean(json_node_t* node, boolean_t val);

LIBEXPORT int json_set_number(json_node_t* node, const char* val);

LIBEXPORT int json_add_node(json_node_t* parent, json_str_t name, json_node_t* node);
LIBEXPORT int json_remove_node(json_node_t* parent, json_node_t* node);

STATIC_INLINE int json_add_integer(json_node_t* parent, json_str_t name, int64_t val) {
	return json_add_node(parent, name, json_new_integer(val));
}
STATIC_INLINE int json_add_double(json_node_t* parent, json_str_t name, double val) {
	return json_add_node(parent, name, json_new_double(val));
}
STATIC_INLINE int json_add_string(json_node_t* parent, json_str_t name, json_str_t val) {
	return json_add_node(parent, name, json_new_string(val));
}
STATIC_INLINE int json_add_boolean(json_node_t* parent, json_str_t name, boolean_t val) {
	return json_add_node(parent, name, json_new_boolean(val));
}

LIBEXPORT json_node_t* json_copy_node(json_node_t* node);

LIBEXPORT void json_node_destroy(json_node_t* node);

LIBEXPORT int json_parse(json_buffer_t* buf, json_node_t** root);

LIBEXPORT size_t json_write_count(json_node_t* node, boolean_t formatted);
LIBEXPORT int json_write_buf(json_node_t* node, char* buf, size_t len, boolean_t formatted);
LIBEXPORT int json_write_file(json_node_t* node, FILE* file, boolean_t formatted);

LIBEXPORT int json_init(void);
LIBEXPORT void json_fini(void);

#endif /* JSON_H_ */

