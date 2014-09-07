
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



#ifndef JSONIMPL_H_
#define JSONIMPL_H_

#include <defs.h>
#include <atomic.h>

#include <json.h>

#include <stdlib.h>
#include <stdarg.h>

struct json_parser {
	int	lineno;			/* Number of line */
	int newline;		/* Last index of newline character */

	int index;
};

struct json_writer {
	int  (* writer_error)(void* state);
	void (* write_byte)(void* state, char byte);
	void (* write_string)(void* state, const char* str);
	void (* write_byte_array)(void* state, const char* array, size_t sz);
};

int json_write_impl(json_node_t* node, struct json_writer* writer, void* state, boolean_t formatted, int indent);

void json_buf_free(json_buffer_t* buf);

STATIC_INLINE void json_buf_hold(json_buffer_t* buf) {
	atomic_inc(&buf->ref_count);
}

STATIC_INLINE void json_buf_rele(json_buffer_t* buf) {
	if(atomic_dec(&buf->ref_count) == 1l) {
		json_buf_free(buf);
	}
}

json_str_t json_str_reference(json_buffer_t* buf, int from, int to);
void json_str_free(json_str_t json_str, json_buffer_t* buf);

json_node_t* json_node_create(json_buffer_t* buf, json_type_t type);
json_node_t* json_node_create_copy(json_node_t* node);

int json_set_error_va(struct json_parser* parser, int error, const char* fmt, va_list va);

STATIC_INLINE int json_set_parser_error(struct json_parser* parser, int error, const char* fmt, ...) {
	va_list va;
	int ret;

	va_start(va, fmt);
	ret = json_set_error_va(parser, error, fmt, va);
	va_end(va);

	return ret;
}

STATIC_INLINE int json_set_error(int errno) {
	/* This is intended to be unitialized, because it will be ignored
	 * by json_set_error_va (fmt == NULL) */
	va_list va;

	return json_set_error_va(NULL, errno, NULL, va);
}

STATIC_INLINE int json_set_error_str(int errno, const char* fmt, ...) {
	va_list va;
	int ret;

	va_start(va, fmt);
	ret = json_set_error_va(NULL, errno, fmt, va);
	va_end(va);

	return ret;
}

int json_init_errors(void);
void json_destroy_errors(void);

int json_parse_number(struct json_parser* parser, json_buffer_t* buf, json_node_t** object);

#endif /* JSONIMPL_H_ */

