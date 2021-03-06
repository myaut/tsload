
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

#include <string.h>


/* Parser error handling
 ------------------------------- */
static thread_key_t		json_error;
static thread_mutex_t	json_error_mutex;
static list_head_t		json_error_head;

json_error_msg_func json_error_msg = NULL;

typedef struct {
	json_error_state_t		state;
	list_node_t				node;
} json_error_handler_t;

const char* json_error_msg_text[] = {
	"OK",
	"Internal error",
	"attribute ?? have invalid type",
	"attribute ?? not found",
	"attribute ?? is unused",
	"integer overflow"
};
const int json_error_msg_count = 5;

void json_register_error_msg_func(json_error_msg_func func) {
	json_error_msg = func;
}

/**
 * Set JSON error. Create error state object if necessary.
 *
 * @param parser 	parser state. may be set to NULL if error is not parser-related
 * @param error 	error code
 * @param fmt		error string format. may be set to NULL
 */
int json_set_error_va(struct json_parser* parser, int error, const char* fmt, va_list va) {
	json_error_handler_t* hdl = tkey_get(&json_error);
	json_error_state_t* state;

	size_t count = 0;

	va_list va1;
	va_list va2;

	va_copy(va1, va);
	va_copy(va2, va);

	if(hdl == NULL) {
		hdl = mp_malloc(sizeof(json_error_handler_t));

		list_node_init(&hdl->node);

		mutex_lock(&json_error_mutex);
		list_add_tail(&hdl->node, &json_error_head);
		mutex_unlock(&json_error_mutex);

		tkey_set(&json_error, hdl);
	}

	state = &hdl->state;

	if(parser) {
		state->je_line = parser->lineno;
		state->je_char = parser->index - parser->newline;
	}

	state->je_error = error;

	if(fmt) {
		count = vsnprintf(state->je_message, JSONERRLEN, fmt, va1);

		if(parser) {
			char msg2[20];
			snprintf(msg2, 20, " at %d:%" PRIsz, state->je_line, state->je_char);

			strncat(state->je_message, msg2, JSONERRLEN - count);
		}
		else {
			if(json_error_msg != NULL) {
				json_error_msg(error, fmt, va2);
			}
		}
	}
	else if(-error < json_error_msg_count) {
		if(error > JSON_MIN_GENERIC_ERROR) {
			strncpy(state->je_message, json_error_msg_text[-error], JSONERRLEN);
		}
		else {
			snprintf(state->je_message, JSONERRLEN, "JSON error %d", error);
		}
	}

	return error;
}

/**
 * Returns JSON error state (for parsers)
 */
json_error_state_t* json_get_error(void) {
	json_error_handler_t* hdl = tkey_get(&json_error);

	if(hdl == NULL)
		return NULL;

	return &hdl->state;
}

/**
 * Returns JSON error number
 */
int json_errno(void) {
	json_error_handler_t* hdl = tkey_get(&json_error);

	if(hdl == NULL)
		return JSON_OK;

	return hdl->state.je_error;
}

/**
 * Returns JSON error message
 */
const char* json_error_message() {
	json_error_handler_t* hdl = tkey_get(&json_error);

	if(hdl == NULL || hdl->state.je_error == JSON_OK) {
		return "OK";
	}

	return hdl->state.je_message;
}

/**
 * Clears JSON error number (sets to JSON_OK)
 */
void json_errno_clear(void) {
	json_error_handler_t* hdl = tkey_get(&json_error);

	if(hdl == NULL)
		return;

	hdl->state.je_error = JSON_OK;
}

int json_init_errors(void) {
	tkey_init(&json_error, "json_error");
	mutex_init(&json_error_mutex, "json_error_mutex");

	list_head_init(&json_error_head, "json_error_head");

	return 0;
}

void json_destroy_errors(void) {
	json_error_handler_t* hdl;
	json_error_handler_t* next;

	list_for_each_entry_safe(json_error_handler_t, hdl, next,
							 &json_error_head, node) {
		mp_free(hdl);
	}

	mutex_destroy(&json_error_mutex);
	tkey_destroy(&json_error);
}

