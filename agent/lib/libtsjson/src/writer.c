
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



#include <mempool.h>

#include <json.h>
#include <jsonimpl.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

#define ESCAPE_CHARACTER(esc)								\
	if(p != s)												\
		writer->write_byte_array(state, s, p - s);			\
	writer->write_string(state, esc);						\
	s = p + 1;

int json_write_node_indent = 2;

int json_write_string(json_str_t str, struct json_writer* writer, void* state) {
	const char* s = str;
	const char* p = str;

	unsigned long u;
	char us[8];

	writer->write_byte(state, '"');

	while(*p) {
		switch(*p) {
		case '"':
			ESCAPE_CHARACTER("\\\"");
			break;
		case '\\':
			ESCAPE_CHARACTER("\\\\");
			break;
		case '/':
			ESCAPE_CHARACTER("\\/");
			break;
		case '\n':
			ESCAPE_CHARACTER("\\n");
			break;
		case '\t':
			ESCAPE_CHARACTER("\\t");
			break;
		case '\r':
			ESCAPE_CHARACTER("\\r");
			break;
		case '\f':
			ESCAPE_CHARACTER("\\f");
			break;
		case '\b':
			ESCAPE_CHARACTER("\\b");
			break;
		default:
			if(*p > 0x80) {
				/* Unicode */
				u = 0;
				if((*p & 0xe0) == 0xe0) {
					/* n = 3 */
					if((p[1] & 0x80) != 0x80 || (p[2] & 0x80) != 0x80)
						goto unicode1;
					u =  ((p[0] & 0x0f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f);
					p += 2;
				}
				else if((*p & 0xc0) == 0xc0) {
					/* n = 2 */
					if((p[1] & 0x80) != 0x80)
						goto unicode1;
					u = ((p[0] & 0x1f) << 6) | (p[1] & 0x3f);
					++p;
				}
				else {
					goto unicode1;
				}
				snprintf(us, 8, "\\u%04lx", u);
				ESCAPE_CHARACTER(us);
			}
			break;
		}

		++p;
	}

	if(p != s)
		writer->write_byte_array(state, s, p - s);
	writer->write_byte(state, '"');

	return writer->writer_error(state);

unicode1:
	return json_set_error_str(JSON_INVALID_UNICODE_CHR, "Invalid Unicode byte '%x'", *p);
}

#undef ESCAPE_CHARACTER

int json_write_number(json_node_t* num, struct json_writer* writer, void* state) {
	/* Use E-format for laaaaaarge doubles, so it wouldn't be overflown */
	char buf[32];
	int count;

	if(num->jn_is_integer) {
		count = snprintf(buf, 32, "%" PRId64, num->jn_data.i);
	}
	else {
		double d = num->jn_data.d;

		if(labs((long) log10(d)) > 17) {
			count = snprintf(buf, 32, "%.20e", d);
		}
		else {
			count = snprintf(buf, 32, "%f", d);
		}
	}

	writer->write_string(state, buf);

	return writer->writer_error(state);
}

int json_write_impl(json_node_t* node, struct json_writer* writer, void* state, boolean_t formatted, int indent) {
	boolean_t is_node = B_FALSE;
	json_node_t* child;

	int ret;
	char* indent_str = NULL;
	int orig_indent = indent;

	switch(node->jn_type) {
	case JSON_NODE:
		is_node = B_TRUE;
	case JSON_ARRAY:
		indent += json_write_node_indent;
		indent_str = mp_malloc(indent);
		memset(indent_str, ' ', indent);

		if(formatted && indent > json_write_node_indent) {
			writer->write_byte_array(state, indent_str, orig_indent);
			writer->write_byte(state, '\n');
		}

		writer->write_byte(state, is_node? '{' : '[');

		if(formatted)
			writer->write_byte(state, '\n');

		list_for_each_entry(json_node_t, child, &node->jn_child_head, jn_child_node) {
			if(formatted) {
				writer->write_byte_array(state, indent_str, indent);
			}

			if(is_node) {
				json_write_string(child->jn_name, writer, state);
				writer->write_byte(state, ':');

				if(formatted)
					writer->write_byte(state, ' ');
			}

			ret = json_write_impl(child, writer, state, formatted, indent);
			if(ret != JSON_OK) {
				goto end;
			}

			if(!list_is_last(&child->jn_child_node, &node->jn_child_head)) {
				writer->write_byte(state, ',');
			}

			if(formatted) {
				writer->write_byte(state, '\n');
			}
		}

		if(formatted && indent > json_write_node_indent) {
			writer->write_byte_array(state, indent_str, orig_indent);
		}

		writer->write_byte(state, is_node? '}' : ']');

		if(formatted && orig_indent == 0) {
			writer->write_byte(state, '\n');
		}

		break;
	case JSON_BOOLEAN:
		if(node->jn_data.b) {
			writer->write_string(state, "true");
		}
		else {
			writer->write_string(state, "false");
		}
		break;
	case JSON_NULL:
		writer->write_string(state, "null");
		break;
	case JSON_STRING:
		ret = json_write_string(node->jn_data.s, writer, state);
		break;
	case JSON_NUMBER:
		ret = json_write_number(node, writer, state);
		break;
	}

end:
	if(indent_str) {
		mp_free(indent_str);
	}

	if(ret == JSON_OK)
		return writer->writer_error(state);

	return ret;
}

/* Counter writer - used to calculate number of bytes that would be
 * generated by JSON to allocate correct array for it
 * -------------------------- */

static int json_writer_error_count(void* state) {
	return JSON_OK;
}

static void json_write_byte_count(void* state, char byte) {
	long* count = (long*) state;
	++(*count);
}

static void json_write_string_count(void* state, const char* str) {
	long* count = (long*) state;
	(*count) += strlen(str);
}

static void json_write_byte_array_count(void* state, const char* array, size_t sz) {
	long* count = (long*) state;
	(*count) += sz;
}

struct json_writer json_writer_count = {
	json_writer_error_count,
	json_write_byte_count,
	json_write_string_count,
	json_write_byte_array_count
};

long json_write_count(json_node_t* node, boolean_t formatted) {
	long count = 1;
	int ret = json_write_impl(node, &json_writer_count, &count, formatted, 0);

	if(ret != JSON_OK)
		return ret;

	return count;
}

/* Membuf writer - write to pre-allocated memory buffer
 * -------------------------- */

struct json_buf_state {
	char* buf;
	char* ptr;

	size_t len;
	size_t sz;
};

static int json_writer_error_buf(void* state) {
	struct json_buf_state* buf = (struct json_buf_state*) state;

	if(buf->len >= buf->sz)
		return JSON_BUFFER_OVERFLOW;

	return JSON_OK;
}

static void json_write_byte_buf(void* state, char byte) {
	struct json_buf_state* buf = (struct json_buf_state*) state;

	++buf->len;
	if(buf->len >= buf->sz)
		return;

	*buf->ptr++ = byte;
}

static void json_write_string_buf(void* state, const char* str) {
	struct json_buf_state* buf = (struct json_buf_state*) state;
	size_t len = strlen(str);

	buf->len += len;
	if(buf->len >= buf->sz)
		return;

	strcpy(buf->ptr, str);
	buf->ptr += len;
}

static void json_write_byte_array_buf(void* state, const char* array, size_t sz) {
	struct json_buf_state* buf = (struct json_buf_state*) state;

	buf->len += sz;
	if(buf->len >= buf->sz)
		return;

	memcpy(buf->ptr, array, sz);
	buf->ptr += sz;
}

struct json_writer json_writer_buf = {
	json_writer_error_buf,
	json_write_byte_buf,
	json_write_string_buf,
	json_write_byte_array_buf
};

int json_write_buf(json_node_t* node, char* buf, size_t len, boolean_t formatted) {
	struct json_buf_state state;
	int ret;

	state.buf = buf;
	state.ptr = buf;
	state.len = 0;
	state.sz = len;

	ret = json_write_impl(node, &json_writer_buf, &state, formatted, 0);

	if(state.len < (state.sz - 1))
		*state.ptr = '\0';

	return ret;
}

