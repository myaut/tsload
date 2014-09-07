/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, Tune-IT

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
#include <list.h>

#include <tsobj.h>

tsobj_api_t* tsobj_api_impl = NULL;

tsobj_error_msg_func tsobj_error_msg = NULL;

/* TSObj error handling
 ------------------------------- */
static thread_key_t		tsobj_error;
static thread_mutex_t	tsobj_error_mutex;
static list_head_t		tsobj_error_head;

int tsobj_set_error_va(int error, const char* fmt, va_list va) {
	va_list va1;
	va_list va2;

	tsobj_error_state_t* state = (tsobj_error_state_t*) tkey_get(&tsobj_error);

	va_copy(va1, va);
	va_copy(va2, va);

	if(state == NULL) {
		state = (tsobj_error_state_t*) mp_malloc(sizeof(tsobj_error_state_t));

		list_node_init(&state->tsobj_error_node);

		mutex_lock(&tsobj_error_mutex);
		list_add_tail(&state->tsobj_error_node, &tsobj_error_head);
		mutex_unlock(&tsobj_error_mutex);

		tkey_set(&tsobj_error, state);
	}

	if(tsobj_error_msg != NULL) {
		tsobj_error_msg(error, fmt, va1);
	}

	state->tsobj_error = error;
	vsnprintf(state->tsobj_message, TSOBJERRLEN, fmt, va2);

	return 0;
}

/**
 * Returns tsobj error state (for parsers)
 */
tsobj_error_state_t* tsobj_get_error(void) {
	tsobj_error_state_t* state = (tsobj_error_state_t*) tkey_get(&tsobj_error);

	return state;
}

/**
 * Returns TSObj error number
 */
int tsobj_errno(void) {
	tsobj_error_state_t* state = (tsobj_error_state_t*) tkey_get(&tsobj_error);

	if(state == NULL)
		return TSOBJ_OK;

	return state->tsobj_error;
}

/**
 * Returns TSObj error message
 */
const char* tsobj_error_message() {
	tsobj_error_state_t* state = tkey_get(&tsobj_error);

	if(state == NULL || state->tsobj_error == TSOBJ_OK) {
		return "OK";
	}

	return state->tsobj_message;
}

/**
 * Clears TSObj error number (sets to TSOBJ_OK)
 */
void tsobj_errno_clear(void) {
	tsobj_error_state_t* state = tkey_get(&tsobj_error);

	if(state == NULL)
		return;

	state->tsobj_error = TSOBJ_OK;
}

int tsobj_init(void) {
	tkey_init(&tsobj_error, "tsobj_error");
	mutex_init(&tsobj_error_mutex, "tsobj_error_mutex");

	list_head_init(&tsobj_error_head, "tsobj_error_head");

	/* Promote errors from TSJSON to TSObj */
	json_error_msg = tsobj_set_error_va;

	return 0;
}

void tsobj_fini(void) {
	tsobj_error_state_t* hdl;
	tsobj_error_state_t* next;

	list_for_each_entry_safe(tsobj_error_state_t, hdl, next,
							 &tsobj_error_head, tsobj_error_node) {
		mp_free(hdl);
	}

	mutex_destroy(&tsobj_error_mutex);
	tkey_destroy(&tsobj_error);
}
