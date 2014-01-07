/*
 * tsfutil.h
 *
 *  Created on: Jan 7, 2014
 *      Author: myaut
 */

#ifndef TSFUTIL_H_
#define TSFUTIL_H_

#include <tsfile.h>
#include <stdio.h>

#define COMMAND_CREATE		0
#define COMMAND_ADD			1
#define COMMAND_GET_COUNT	2
#define COMMAND_GET_ENTRIES 3

tsfile_schema_t* schema_read(const char* filename);

typedef int (*tsfutil_get_func)(FILE* file, tsfile_t* ts_file, int start, int end);
typedef int (*tsfutil_add_func)(FILE* file, tsfile_t* ts_file);

typedef struct {
	tsfutil_get_func get;
	tsfutil_add_func add;
} tsfutil_backend_t;

extern tsfutil_backend_t json_backend;
extern tsfutil_backend_t jsonraw_backend;

#endif /* TSFUTIL_H_ */
