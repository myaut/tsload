
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

#include <tsload/mempool.h>

#include <tsload/json/json.h>
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

char json_escape_table[128] = {
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int json_write_string(json_str_t str, struct json_writer* writer, void* state) {
	const char* s = (const char*) str;
	const char* p = (const char*) str;

	unsigned long u;
	char us[8];

	writer->write_byte(state, '"');

	while(*p) {
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
		else if(json_escape_table[(unsigned char) *p] == 1) {
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
				break;
			}
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
	
	if(count > 31) {
		/* This shouldn't happen, but it did. Report as error */
		return json_set_error_str(JSON_BUFFER_OVERFLOW, "Too long number that didn't fit into buffer.");
	}
	
	writer->write_string(state, buf);

	return writer->writer_error(state);
}

int json_write_impl(json_node_t* node, struct json_writer* writer, void* state, boolean_t formatted, int indent) {
	boolean_t is_node = B_FALSE;
	json_node_t* child;

	int ret = JSON_OK;
	char* indent_str = NULL;
	int orig_indent = indent;

	switch(node->jn_type) {
	case JSON_NODE:
		is_node = B_TRUE;
	case JSON_ARRAY:
		indent += json_write_node_indent;
		indent_str = mp_malloc(indent);
		memset(indent_str, ' ', indent);

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
	size_t* count = state;
	++(*count);
}

static void json_write_string_count(void* state, const char* str) {
	size_t* count = state;
	(*count) += strlen(str);
}

static void json_write_byte_array_count(void* state, const char* array, size_t sz) {
	size_t* count = state;
	(*count) += sz;
}

struct json_writer json_writer_count = {
	json_writer_error_count,
	json_write_byte_count,
	json_write_string_count,
	json_write_byte_array_count
};

size_t json_write_count(json_node_t* node, boolean_t formatted) {
	size_t count = 1;
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

/* FILE writer - write to stdio FILE
 * -------------------------- */

struct json_file_state {
	FILE* file;
};

static int json_writer_error_file(void* state) {
	struct json_file_state* file = (struct json_file_state*) state;

	if(ferror(file->file) != 0)
		return JSON_FILE_ERROR;

	return JSON_OK;
}

static void json_write_byte_file(void* state, char byte) {
	struct json_file_state* file = (struct json_file_state*) state;
	fputc(byte, file->file);
}

static void json_write_string_file(void* state, const char* str) {
	struct json_file_state* file = (struct json_file_state*) state;
	fputs(str, file->file);
}

static void json_write_byte_array_file(void* state, const char* array, size_t sz) {
	struct json_file_state* file = (struct json_file_state*) state;
	fwrite(array, sz, 1, file->file);
}

struct json_writer json_writer_file = {
	json_writer_error_file,
	json_write_byte_file,
	json_write_string_file,
	json_write_byte_array_file
};

int json_write_file(json_node_t* node, FILE* file, boolean_t formatted) {
	struct json_file_state state;
	int ret;

	state.file = file;

	ret = json_write_impl(node, &json_writer_file, &state, formatted, 0);

	return ret;
}
