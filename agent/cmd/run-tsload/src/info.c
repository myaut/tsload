/*
 * info.c
 *
 *  Created on: Nov 19, 2012
 *      Author: myaut
 */

#include <tsload.h>

#include <libjson.h>

#include <stdio.h>

int do_info() {
	JSONNODE* node = tsload_get_workload_types();
	char* info = json_write_formatted(node);

	fputs(info, stdout);
	fputc('\n', stdout);

	json_free(info);
	json_delete(node);

	return 0;
}

