
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

#include <stdio.h>


extern int json_test_main(void);

void dump_error(void) {
	json_error_state_t* state = json_get_error();

	if(state) {
		fprintf(stderr, "%s at %d:%" PRIsz "\n", 
				state->je_message, state->je_line, state->je_char);
	}
}

void dump_node(json_node_t* node) {
	switch(node->jn_type) {
	case JSON_STRING:
		fprintf(stderr, "STRING: '%s'\n", json_as_string(node));
		break;
	}
}

int test_main(void) {
	int ret;

	threads_init();
	mempool_init();
	json_init();

	json_test_main();

	json_fini();
	mempool_fini();
	threads_fini();

	return 0;
}

