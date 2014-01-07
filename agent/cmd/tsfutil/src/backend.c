/*
 * backend.c
 *
 *  Created on: Jan 8, 2014
 *      Author: myaut
 */

#define LOG_SOURCE "tsfutil"
#include <log.h>

#include <tsfutil.h>
#include <filemmap.h>

#include <stdio.h>
#include <stdlib.h>

/* TODO: Implement CSV backend */

typedef char* (*json_write_func)(const JSONNODE* node);

size_t tsfutil_json_fragment  = 4096;
size_t tsfutil_json_min_alloc = 16384;

const char* tsfile_error_str[] = {
	"OK",
	"Superblock failure",
	"Data failure",
	"Invalid range"
};

int tsfutil_json_get_impl(FILE* file, tsfile_t* ts_file, int start, int end, json_write_func func) {
	char* str;
	JSONNODE* node;
	int ret = 0;

	if((end - start) == 1) {
		node = json_tsfile_get(ts_file, start);
	}
	else {
		node = json_tsfile_get_array(ts_file, start, end);
	}

	if(node == NULL) {
		logmsg(LOG_CRIT, "Failed to get JSON entries. Error: %s",
			   tsfile_error_str[-tsfile_errno]);
		return 1;
	}

	str = func(node);

	if(str == NULL) {
		ret = 1;
	}
	else {
		fputs(str, file);
		json_free(str);
	}

	if((end - start) == 1) {
		json_tsfile_put(ts_file, node);
	}
	else {
		json_tsfile_put_array(ts_file, node);
	}

	return ret;
}

int tsfutil_json_get(FILE* file, tsfile_t* ts_file, int start, int end) {
	return tsfutil_json_get_impl(file, ts_file, start, end, json_write_formatted);
}

int tsfutil_jsonraw_get(FILE* file, tsfile_t* ts_file, int start, int end) {
	return tsfutil_json_get_impl(file, ts_file, start, end, json_write);
}

int tsfutil_json_add(FILE* file, tsfile_t* ts_file) {
	size_t size = tsfutil_json_min_alloc;
	size_t length = 0;
	size_t read_len;
	char* str = malloc(tsfutil_json_min_alloc);
	char* ptr = str;

	int err = 0;

	JSONNODE* node;

	/* Read everything from file into str
	 * Couldn't memory map file because file may be stdin */
	while(feof(file) == 0) {
		read_len = fread(ptr, 1, tsfutil_json_fragment, file);
		if(read_len == 0)
			break;

		length += read_len;
		ptr += read_len;

		if((size - length - 1) < tsfutil_json_fragment) {
			size += tsfutil_json_fragment;
			str = realloc(str, size);
			ptr = str + length;

			if(str == NULL) {
				logmsg(LOG_CRIT, "Failed to reallocate %ld bytes", size);
				return 1;
			}
		}
	}

	str[length + 1] = '\0';

	node = json_parse(str);
	free(str);

	if(node == NULL) {
		logmsg(LOG_CRIT, "Couldn't parse JSON");
		return 1;
	}

	if(json_type(node) == JSON_ARRAY) {
		err = json_tsfile_add_array(ts_file, node);
	}
	else {
		err = json_tsfile_add(ts_file, node);
	}
	json_delete(node);

	if(err != TSFILE_OK) {
		logmsg(LOG_CRIT, "Failed to add JSON entries. Error: %s",
			   tsfile_error_str[-tsfile_errno]);
		return 1;
	}

	return 0;
}

tsfutil_backend_t json_backend = {
	tsfutil_json_get, tsfutil_json_add
};

tsfutil_backend_t jsonraw_backend = {
	tsfutil_jsonraw_get, tsfutil_json_add
};
