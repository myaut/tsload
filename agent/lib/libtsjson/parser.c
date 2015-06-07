
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
#include <tsload/list.h>
#include <tsload/ilog2.h>

#include <tsload/json/json.h>
#include <jsonimpl.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>


int json_parse_value(struct json_parser* parser, json_buffer_t* buf, json_node_t** object);

#define GET_STATE(idx, data)		\
	do {							\
		idx = parser->index;		\
		data = buf->buffer + idx;	\
	} while(0)

#define PUT_STATE(idx, data)		\
	parser->index = idx

#define PUT_STATE_DATA(idx, data)	\
	parser->index = (data - buf->buffer)

#define INC_STATE()					\
	++parser->index

static char json_whitespace_table[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 1, 1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

#define JSON_NOTSPACE	0
#define JSON_NEWLINE	2
#define JSON_SPACE		1

/**
 * Skip whitespace characters.
 * @return B_TRUE if there are still symbols inside buffer
 */

static boolean_t skip_whitespace(struct json_parser* parser, json_buffer_t* buf) {
	size_t idx; char* data;
	char mode = JSON_SPACE;

	GET_STATE(idx, data);

	while(idx < buf->size) {
		mode = json_whitespace_table[(unsigned char) *data];

		if(mode == JSON_NEWLINE) {
			++parser->lineno;
			parser->newline = idx;
		}

		if(mode == JSON_NOTSPACE)
			break;

		++data; ++idx;
	}

	PUT_STATE(idx, data);

	return idx < buf->size;
}

static int hex_char_to_int(char c) {
	if(c >= '0' && c <= '9')
		return c - '0';

	if(c >= 'a' && c <= 'f')
		return c - 'a' + 10;

	if(c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return -1;
}

static int unicode_from_hex(char* s) {
	int u = hex_char_to_int(s[0]);
	int l = hex_char_to_int(s[1]);

	if(l == -1 || u == -1)
		return -1;

	return u << 4 | l;
}

/**
 * Unescape string - convert \\n to newlines and so on
 *
 * @return Negative value if error was occured (error code) or number of characters \
 * 		   that was cut out from buffer
 */
static int unescape_string(struct json_parser* parser, json_buffer_t* buf, size_t idx, size_t to) {
	char* str = buf->buffer + idx;
	char* end = buf->buffer + to;
	char* dst = str;

	int count = 0;
	int c1, c2;
	long ch;

	while(str < end) {
		if(*str == '\\') {
			++str; ++count;

			/* May go out of buffer here, but \\ shouldn't be at
			 * the end of the string because it escape " and being handled by
			 * json_parse_string() */

			switch(*str) {
			case 'n':
				*dst = '\n';
				break;
			case 'b':
				*dst = '\b';
				break;
			case 'f':
				*dst = '\f';
				break;
			case 'r':
				*dst = '\r';
				break;
			case 't':
				*dst = '\t';
				break;
			case 'u': {
					/* FIXME: This code assumes UTF-8 as encoding */

					if(str <= (end - 5)) {
						c1 = unicode_from_hex(str + 1);
						c2 = unicode_from_hex(str + 3);
						ch = c1 << 8 | c2;

						if(c1 > 0 && c2 > 0) {
							/* Decode as UTF-8 */
							if(ch < 0x80) {
								count += 4;
								*dst   = (char) ch;
							}
							else if(ch < 0x0800) {
								count += 3;
								*dst++ = (char)(0xc0 | (ch >> 6));
								*dst   = (char)(0x80 | (ch & 0x3f));
							}
							else {
								if(0xD800 <= ch && ch <= 0xDBFF) {
									/* TODO: Implement surrogate unicode sequences */
									PUT_STATE_DATA(idx, str);
									return json_set_parser_error(parser, JSON_STRING_INVALID_ESC,
																 "Surrogate unicode sequence while parsing STRING");
								}

								count += 2;
								*dst++ = (char)(0xe0 | (ch >> 12));
								*dst++ = (char)(0x80 | ((ch >> 6) & 0x3f));
								*dst   = (char)(0x80 | (ch & 0x3f));
							}
						}
						else {
							PUT_STATE_DATA(idx, str);
							return json_set_parser_error(parser, JSON_STRING_INVALID_ESC,
												  	     "Invalid unicode sequence while parsing STRING");
						}
					}
					else {
						PUT_STATE_DATA(idx, str);
						return json_set_parser_error(parser, JSON_END_OF_BUFFER,
												     "Unicode sequence is out of bounds while parsing STRING");
					}

					str += 4;
				}
				break;
			case '\\':
			case '/':
			case '"':
				*dst = *str;
				break;
			default:
				PUT_STATE_DATA(idx, str);
				return json_set_parser_error(parser, JSON_STRING_INVALID_ESC,
											 "Invalid escape '%c' while parsing STRING", *str);
			}
		}
		else if(str != dst) {
			*dst = *str;
		}

		++str;
		++dst;
	}

	return count;
}

int json_parse_string(struct json_parser* parser, json_buffer_t* buf, json_str_t* string) {
	size_t idx; char* data;
	size_t from, to;
	int ret;

	boolean_t have_escaped = B_FALSE;

	INC_STATE();
	GET_STATE(idx, data);
	from = idx;

	while(*data != '"') {
		/* TODO: check other characters as well */
		if(*data == '\n') {
			PUT_STATE(idx, data);
			return json_set_parser_error(parser, JSON_STRING_UNESCAPED,
										 "Unescaped character '%d' while parsing STRING", *data);
		}

		if(*data == '\\') {
			++idx; ++data;
			have_escaped = B_TRUE;
		}

		if(idx >= buf->size) {
			PUT_STATE(idx, data);
			return json_set_parser_error(parser, JSON_END_OF_BUFFER,
										 "Unexpected end of buffer while parsing STRING");
		}

		++idx; ++data;
	}

	to = idx;

	++idx;
	PUT_STATE(idx, data);

	if(have_escaped) {
		ret = unescape_string(parser, buf, from, to);

		if(ret < 0)
			return ret;

		to -= ret;
	}

	*string = json_str_reference(buf, from, to);

	return JSON_OK;
}

/*
 * JSON doesn't distinguish floats/integers while C (and most languages do)
 * This array helps to parse integer/float literals from JSON - if character
 * is correct for it, it will give 2 for float-only literal and 1 for
 * integer literal. Index is an ASCII code.
 *
 * After all if char with mode 2 (exp or frac) is met, it is parsed as floating
 * point, if not - it will be treated as integer.
 *
 *
 */
char json_number_chr_table[128] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 4, 1,  /* + - . */
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1,  /* 0 1 2 3 4 5 6 7 8 9 */
        1, 1, 1, 1, 1, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* E */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* e */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

#define JSON_NUM_MODE_INVAL	0x1
#define JSON_NUM_MODE_INT	0x2
#define JSON_NUM_MODE_FLOAT	0x4

static char number_table_get(int c) {
	if(c < 128  && c > 0)
		return json_number_chr_table[c];
	return JSON_NUM_MODE_INVAL;
}

int json_parse_number(struct json_parser* parser, json_buffer_t* buf, json_node_t** object) {
	int mode = 0;

	const char* literal;
	char* endptr;

	double d;
	int64_t i;

	size_t idx; char* data;
	GET_STATE(idx, data);

	errno = 0;
	literal = data;

	do {
		mode |= number_table_get(*data);

		if(unlikely(mode & JSON_NUM_MODE_INVAL))
			break;

		++idx; ++data;
	} while(idx < buf->size);

	PUT_STATE(idx, data);

	if(mode & JSON_NUM_MODE_FLOAT) {
		d = strtod(literal, &endptr);
	}
	else {
		i = strtoll(literal, &endptr, 10);
	}

	if(errno == ERANGE) {
		PUT_STATE(idx, data);
		return json_set_parser_error(parser, JSON_NUMBER_OVERFLOW,
									 "NUMBER literal '%s' is too large", literal);
	}

	if(endptr != data) {
		PUT_STATE(idx, data);
		return json_set_parser_error(parser, JSON_NUMBER_INVALID,
									 "NUMBER literal '%s' is invalid", literal);
	}

	if(*object == NULL) {
		*object = json_node_create(buf, JSON_NUMBER);
	}

	if(mode & JSON_NUM_MODE_FLOAT) {
		(*object)->jn_is_integer = B_FALSE;
		(*object)->jn_data.d = d;
	}
	else {
		(*object)->jn_is_integer = B_TRUE;
		(*object)->jn_data.i = i;
	}

	return JSON_OK;
}

int json_parse_node(struct json_parser* parser, json_buffer_t* buf, json_node_t** object, boolean_t is_array) {
	json_str_t name = NULL;
	json_node_t* value = NULL;

	size_t idx; char* data;
	int ret = JSON_OK;

	char end_char = (is_array)? ']' : '}';

	*object = json_node_create(buf, (is_array)? JSON_ARRAY : JSON_NODE);

	INC_STATE();

	if(!skip_whitespace(parser, buf))
		goto eob;

	GET_STATE(idx, data);

	while(*data != end_char) {
		if(!is_array) {
			/* For nodes - parse element name */
			if(*data != '"') {
				PUT_STATE(idx, data);
				ret = json_set_parser_error(parser, JSON_OBJECT_INVALID,
					 	 	 	 	 	 	 "Unexpected char '%c' while parsing OBJECT name - "
					 	 	 	 	 	 	 "expected QUOTE", *data);
				goto error;
			}

			ret = json_parse_string(parser, buf, &name);
			if(ret != JSON_OK)
				goto error;

			if(!skip_whitespace(parser, buf))
				goto eob;

			GET_STATE(idx, data);

			if(*data != ':') {
				PUT_STATE(idx, data);
				ret = json_set_parser_error(parser, JSON_OBJECT_INVALID,
											 "Unexpected char '%c' while parsing OBJECT name - "
											 "expected COLON", *data);
				goto error;
			}

			INC_STATE();
		}

		/* Parse value and add it to node */
		ret = json_parse_value(parser, buf, &value);
		if(ret != JSON_OK)
			goto error;

		if(!skip_whitespace(parser, buf))
			goto eob;

		json_add_node(*object, name, value);
		name = NULL;
		value = NULL;

		GET_STATE(idx, data);

		/* Go to next element if possible */
		if(*data == end_char)
			break;

		if(*data != ',') {
			PUT_STATE(idx, data);
			ret = json_set_parser_error(parser, JSON_OBJECT_INVALID,
										 "Unexpected char '%c' while parsing %s "
										 "expected COMMA or RIGHT BRACKET '%c'",
										 *data, (is_array)? "ARRAY": "OBJECT", end_char);
			goto error;
		}

		INC_STATE();

		if(!skip_whitespace(parser, buf))
			goto eob;

		GET_STATE(idx, data);
	}

	INC_STATE();

	return JSON_OK;

eob:
	ret = json_set_parser_error(parser, JSON_END_OF_BUFFER,
							 	 "Unexpected end of buffer while parsing %s",
							 	(is_array)? "ARRAY": "OBJECT");
error:
	if(name != NULL)
		json_str_free(name, buf);
	if(value != NULL)
		json_node_destroy(value);

	json_node_destroy(*object);
	*object = NULL;

	return ret;
}

int json_parse_literal(struct json_parser* parser, json_buffer_t* buf, const char* literal, size_t len) {
	size_t idx; char* data;

	GET_STATE(idx, data);

	if(idx >= (buf->size - len)) {
		return json_set_parser_error(parser, JSON_END_OF_BUFFER,
									 "Unexpected end of buffer while parsing LITERAL");
	}

	if(strncmp(literal, data, len) != 0) {
		char invallit[5];
		strncpy(invallit, data, len);
		return json_set_parser_error(parser, JSON_LITERAL_INVALID, "Invalid LITERAL '%s'", invallit);
	}

	idx += len; data += len;

	PUT_STATE(idx, data);

	return JSON_OK;
}

int json_parse_value(struct json_parser* parser, json_buffer_t* buf, json_node_t** object) {
	size_t idx; char* data;

	int ret;
	json_str_t str;

	if(!skip_whitespace(parser, buf)) {
		return json_set_parser_error(parser, JSON_END_OF_BUFFER,
									 "Unexpected end of buffer while parsing VALUE");
	}

	GET_STATE(idx, data);

	switch(*data) {
	case '{':
		return json_parse_node(parser, buf, object, B_FALSE);
	case '[':
		return json_parse_node(parser, buf, object, B_TRUE);
	case '"':
		ret = json_parse_string(parser, buf, &str);

		if(ret != JSON_OK)
			return ret;

		*object = json_node_create(buf, JSON_STRING);
		(*object)->jn_data.s = str;

		return JSON_OK;
	case 't':	/* true */
		ret = json_parse_literal(parser, buf, "true", 4);
		if(ret != JSON_OK)
			return ret;

		*object = json_node_create(buf, JSON_BOOLEAN);
		(*object)->jn_data.b = B_TRUE;

		return JSON_OK;
	case 'f':	/* false */
		ret = json_parse_literal(parser, buf, "false", 5);
		if(ret != JSON_OK)
			return ret;

		*object = json_node_create(buf, JSON_BOOLEAN);
		(*object)->jn_data.b = B_FALSE;

		return JSON_OK;
	case 'n':	/* null*/
		ret = json_parse_literal(parser, buf, "null", 4);
		if(ret != JSON_OK)
			return ret;

		*object = json_node_create(buf, JSON_NULL);

		return JSON_OK;
	}

	if(number_table_get(*data) == JSON_NUM_MODE_INT)
		return json_parse_number(parser, buf, object);

	return json_set_parser_error(parser, JSON_VALUE_INVALID,
								 "Unexpected character '%c': unknown VALUE literal", *data);
}

int json_parse(json_buffer_t* buf, json_node_t** root) {
	struct json_parser parser;

	int ret;

	parser.lineno = 1;
	parser.newline = 0;
	parser.index = 0;

	*root = NULL;

	json_buf_hold(buf);
	ret = json_parse_value(&parser, buf, root);
	json_buf_rele(buf);

	if(ret != JSON_OK && *root != NULL) {
		json_node_destroy(*root);
		*root = NULL;
	}

	return ret;
}

