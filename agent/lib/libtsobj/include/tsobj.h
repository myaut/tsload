
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

#ifndef TSOBJ_H_
#define TSOBJ_H_

#include <defs.h>

#include <json.h>
#include <errcode.h>

/**
 * @module TSObject
 *
 * TSObject provides a common layer for accessing external configurations passed
 * to TSLoad core. It works on top of libtsjson, so by default it works with JSON
 * objects. It also have fully compatible interface with TSJSON.
 *
 * However, API object may be overriden, thus all calls will be passed to that API.
 * It may be XML, or external language bindings, for example Java Objects + JNI.
 *
 * It is also provides common layer for error handling, if passed object is incorrect.
 */

#define	TSOBJERRLEN	256

typedef struct tsobj_error_state {
	int   tsobj_error;
	char  tsobj_message[TSOBJERRLEN];

	list_node_t tsobj_error_node;
} tsobj_error_state_t;

#define TSOBJ_OK				JSON_OK
#define TSOBJ_INTERNAL_ERROR	JSON_INTERNAL_ERROR
#define TSOBJ_INVALID_TYPE		JSON_INVALID_TYPE
#define TSOBJ_NOT_FOUND			JSON_NOT_FOUND
#define TSOBJ_NOT_CHILD			JSON_NOT_CHILD

typedef int (*tsobj_error_msg_func)(int errno, const char* format, va_list va);
LIBIMPORT tsobj_error_msg_func tsobj_error_msg;

LIBEXPORT int tsobj_errno(void);
LIBEXPORT void tsobj_errno_clear(void);
LIBEXPORT const char* tsobj_error_message();
LIBEXPORT tsobj_error_state_t* tsobj_get_error(void);

STATIC_INLINE ts_errcode_t tsobj_error_code() {
	int error = tsobj_errno();

	switch(error) {
	case TSOBJ_INVALID_TYPE:
		return TSE_INVALID_TYPE;
	case TSOBJ_NOT_FOUND:
		return TSE_MISSING_ATTRIBUTE;
	case TSOBJ_OK:
		return TSE_OK;
	}

	return TSE_INTERNAL_ERROR;
}

#define TSOBJ_STR				JSON_STR
#define TSOBJ_C_STR				JSON_C_STR

typedef void tsobj_node_t;
typedef json_type_t tsobj_type_t;
typedef json_str_t tsobj_str_t;

#define TSOBJ_GEN
#include <tsobjgen.h>
#undef TSOBJ_GEN

#define DEFINE_TYPED_GET_INTEGER(suffix, type)								\
	STATIC_INLINE int tsobj_get_integer_ ## suffix(tsobj_node_t* parent, 	\
				const char* name, type* val) {								\
		int64_t i64;														\
		int error = tsobj_get_integer_i64(parent, name, &i64);				\
		*val = (type) i64;													\
		return error;														\
	}

DEFINE_TYPED_GET_INTEGER(u8, uint8_t);
DEFINE_TYPED_GET_INTEGER(u16, uint16_t);
DEFINE_TYPED_GET_INTEGER(u32, uint32_t);

DEFINE_TYPED_GET_INTEGER(i8, int8_t);
DEFINE_TYPED_GET_INTEGER(i16, int16_t);
DEFINE_TYPED_GET_INTEGER(i32, int32_t);

DEFINE_TYPED_GET_INTEGER(i, int);
DEFINE_TYPED_GET_INTEGER(l, long);
DEFINE_TYPED_GET_INTEGER(ll, long long);

DEFINE_TYPED_GET_INTEGER(u, unsigned);
DEFINE_TYPED_GET_INTEGER(ul, unsigned long);

DEFINE_TYPED_GET_INTEGER(tm, ts_time_t);

#undef DEFINE_TYPED_GET_INTEGER

/**
 * Iterate over children of TSOBJ node
 *
 * @param parent	parent node
 * @param node		iterator
 * @param id		int variable that contains index of child
 */
#define tsobj_for_each(parent, node, id) 			\
			for(node = tsobj_first(parent, &id);		\
				!tsobj_is_end(parent, node, &id);	\
				node = tsobj_next(node, &id))

LIBEXPORT int tsobj_init(void);
LIBEXPORT void tsobj_fini(void);

typedef tsobj_node_t* (*hm_tsobj_formatter)(hm_item_t* object);
typedef tsobj_str_t (*hm_tsobj_key_formatter)(hm_item_t* object);

struct hm_fmt_context {
	hashmap_t* hm;
	tsobj_node_t* node;

	hm_tsobj_formatter formatter;
	hm_tsobj_key_formatter key_formatter;
};

LIBEXPORT tsobj_node_t* tsobj_hm_format_bykey(hashmap_t* hm, hm_tsobj_formatter formatter, void* key);
LIBEXPORT tsobj_node_t* tsobj_hm_format_all(hashmap_t* hm, hm_tsobj_formatter formatter,
		hm_tsobj_key_formatter key_formatter);

#endif
