/*
 * schema.c
 *
 *  Created on: Jan 7, 2014
 *      Author: myaut
 */

#define LOG_SOURCE "tsfutil"
#include <log.h>

#include <tsfile.h>
#include <libjson.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <assert.h>

tsfile_schema_t* schema_read(const char* filename) {
	JSONNODE* root;
	tsfile_schema_t* schema;

	FILE* file = fopen(filename, "r");
	char* schema_str;
	size_t filesize;

	if(file == NULL) {
		logmsg(LOG_CRIT, "Failed to open schema file '%s'", filename);
		logerror();
		return NULL;
	}

	/* Read schema into array */
	fseek(file, 0, SEEK_END);
	filesize = ftell(file);
	fseek(file, 0, SEEK_SET);

	schema_str = mp_malloc(filesize);
	fread(schema_str, 1, filesize, file);

	fclose(file);

	root = json_parse(schema_str);
	mp_free(schema_str);

	if(root == NULL) {
		logmsg(LOG_CRIT, "Couldn't parse schema file '%s'", filename);
		return NULL;
	}

	schema = json_tsfile_schema_proc(root, B_FALSE);

	json_delete(root);
	return schema;
}
