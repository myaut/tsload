
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
#include <threads.h>

#include <json.h>
#include <jsonimpl.h>

#include <string.h>

/* Parser error handling
 ------------------------------- */
static thread_key_t		json_error;
static thread_mutex_t	json_error_mutex;
static list_head_t		json_error_head;

typedef struct {
	json_error_state_t		state;
	list_node_t				node;
} json_error_handler_t;

const char* json_error_msg[] = {
	"OK",
	"Internal error",
	"VALUE have invalid type",
	"VALUE ?? not found"
};
const int json_error_msg_count = 4;

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
		vsnprintf(state->je_message, JSONERRLEN, fmt, va);
	}
	else if(-error < json_error_msg_count) {
		strncpy(state->je_message, json_error_msg[-error], JSONERRLEN);
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

