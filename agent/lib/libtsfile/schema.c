
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



#define LOG_SOURCE "tsfile"
#include <tsload/log.h>

#include <tsload/defs.h>

#include <tsload/mempool.h>

#include <tsload/json/json.h>

#include <tsfile.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>


#define PARSE_SCHEMA_PARAM(root, node, name, type) 							\
	node = json_find_opt(root, name);										\
	if(node == NULL || json_type(node) != type) {							\
		tsfile_error_msg(TSE_MESSAGE_FORMAT, 								\
						 "Missing or invalid param '%s' in schema", name);	\
		return NULL;														\
	}

#define PARSE_SCHEMA_PARAM_OPT(root, node, name, type) 						\
	node = json_find_opt(root, name);										\
	if(node != NULL && json_type(node) != type) {							\
		tsfile_error_msg(TSE_MESSAGE_FORMAT, 								\
						 "Invalid param '%s' in schema", name);				\
		return NULL;														\
	}

#define PARSE_SCHEMA_FIELD(root, node, name, type, error) 				\
	node = json_find_opt(root, name);									\
	if(node == NULL || json_type(node) != type) {						\
		return error; 													\
	}

static int json_tsfile_schema_proc_field(tsfile_field_t* field, json_node_t* node, ptrdiff_t offset);

extern tsfile_error_msg_func tsfile_error_msg;

tsfile_schema_t* tsfile_schema_read(const char* filename) {
	json_node_t* root;
	json_buffer_t* buf;

	tsfile_schema_t* schema;

	int ret;

	buf = json_buf_from_file(filename);

	if(buf == NULL) {
		return NULL;
	}

	ret = json_parse(buf, &root);

	if(ret != JSON_OK) {
		tsfile_error_msg(TSE_MESSAGE_FORMAT,
				"Couldn't parse schema file '%s': %s",
			   filename, json_error_message());
		return NULL;
	}

	schema = json_tsfile_schema_proc(root, B_FALSE);

	json_node_destroy(root);
	return schema;
}

int tsfile_schema_write(const char* filename, tsfile_schema_t* schema) {
	json_node_t* schema_node;

	FILE* schema_file = fopen(filename, "w");

	if(schema_file == NULL) {
		return -1;
	}

	schema_node = json_tsfile_schema_format(schema);

	json_write_file(schema_node, schema_file, B_TRUE);

	fclose(schema_file);

	return 0;
}

tsfile_schema_t* tsfile_schema_clone(tsfile_schema_t* base) {
	tsfile_schema_t* schema;
	
	size_t schema_size = sizeof(tsfile_shdr_t) + base->hdr.count * sizeof(tsfile_field_t);
	if(schema_size > sizeof(tsfile_schema_t))
		return NULL;
	
	schema = mp_malloc(sizeof(tsfile_schema_t));
	if(schema == NULL)
		return NULL;
	
	memcpy(schema, base, schema_size);
	memset(((char*)schema)+schema_size, 0, sizeof(tsfile_schema_t)-schema_size);
	strncpy(schema->name, base->name, MAXSCHEMANAMELEN);

	return schema;
}

tsfile_schema_t* json_tsfile_schema_proc(json_node_t* root, boolean_t auto_offset) {
	tsfile_schema_t* schema = NULL;
	tsfile_field_t* schema_field;
	ptrdiff_t offset = (ptrdiff_t) -1;
	unsigned field_count, fi;
	int err;

	json_node_t *size, *fields, *field, *name;
	int jid;

	PARSE_SCHEMA_PARAM(root, size, "entry_size", JSON_NUMBER);
	PARSE_SCHEMA_PARAM(root, fields, "fields", JSON_NODE);
	PARSE_SCHEMA_PARAM_OPT(root, name, "name", JSON_STRING);

	field_count = (unsigned) json_size(fields);
	if(field_count > MAXFIELDCOUNT) {
		tsfile_error_msg(TSE_MESSAGE_FORMAT, "Too many fields in schema");
		return NULL;
	}

	schema = mp_malloc(sizeof(tsfile_schema_t));
	memset(schema, 0, sizeof(tsfile_schema_t));

	schema->hdr.entry_size = json_as_integer(size);
	schema->hdr.count = field_count;
	
	memset(schema->name, 0, MAXSCHEMANAMELEN);
	if(name != NULL) {
		strncpy(schema->name, json_as_string(name), MAXSCHEMANAMELEN);
	}

	if(auto_offset) {
		offset = 0;
	}

	field = json_first(fields, &jid);

	for(fi = 0; fi < field_count; ++fi) {
		assert(!json_is_end(fields, field, &jid));

		schema_field = &schema->fields[fi];

		err = json_tsfile_schema_proc_field(schema_field, field, offset);
		if(err != SCHEMA_FIELD_OK) {
			tsfile_error_msg(TSE_MESSAGE_FORMAT,
							 "Failed to parse field '%s': error %d", json_name(field), err);

			mp_free(schema);
			return NULL;
		}

		if(auto_offset) {
			offset += schema_field->size;
		}

		field = json_next(field, &jid);
	}

	return schema;
}

static int json_tsfile_schema_proc_field(tsfile_field_t* field, json_node_t* node, ptrdiff_t field_offset) {
	json_node_t *type, *offset, *size;
	const char* field_type;
	boolean_t need_size = B_TRUE;

	strncpy(field->name, json_name(node), MAXFIELDLEN);

	/* Parse field type */
	PARSE_SCHEMA_FIELD(node, type, "type", JSON_STRING,
							SCHEMA_FIELD_MISSING_TYPE);

	field_type = json_as_string(type);
	if(strcmp(field_type, "bool") == 0) {
		field->type = TSFILE_FIELD_BOOLEAN;
		need_size = B_FALSE;
	}
	else if(strcmp(field_type, "int") == 0) {
		field->type = TSFILE_FIELD_INT;
	}
	else if(strcmp(field_type, "float") == 0) {
		field->type = TSFILE_FIELD_FLOAT;
	}
	else if(strcmp(field_type, "str") == 0) {
		field->type = TSFILE_FIELD_STRING;
	}
	else if(strcmp(field_type, "start_time") == 0) {
		field->type = TSFILE_FIELD_START_TIME;
	}
	else if(strcmp(field_type, "end_time") == 0) {
		field->type = TSFILE_FIELD_END_TIME;
	}
	else {
		return SCHEMA_FIELD_INVALID_TYPE;
	}

	/* Parse field offset */
	if(field_offset == ((ptrdiff_t)-1)) {
		PARSE_SCHEMA_FIELD(node, offset, "offset", JSON_NUMBER,
								SCHEMA_FIELD_MISSING_OFF);
		field->offset = (ptrdiff_t) json_as_integer(offset);
	}
	else {
		field->offset = field_offset;
	}

	/* Parse field size */
	if(need_size) {
		PARSE_SCHEMA_FIELD(node, size, "size", JSON_NUMBER,
								SCHEMA_FIELD_MISSING_SIZE);
		field->size = (size_t) json_as_integer(size);
	}

	return SCHEMA_FIELD_OK;
}

json_node_t* json_tsfile_schema_format(tsfile_schema_t* schema) {
	json_node_t* node = json_new_node(NULL);
	json_node_t* fields = json_new_node(NULL);

	json_node_t* field_node = NULL;
	tsfile_field_t* field;

	int fi;

	json_add_node(node, JSON_STR("fields"), fields);
	json_add_integer(node, JSON_STR("entry_size"), schema->hdr.entry_size);
	json_add_string(node, JSON_STR("name"), json_str_create(schema->name));

	for(fi = 0; fi < schema->hdr.count; ++fi) {
		field = &schema->fields[fi];

		field_node = json_new_node(NULL);

		if(field->type == TSFILE_FIELD_BOOLEAN) {
			json_add_string(field_node, JSON_STR("type"), JSON_STR("bool"));
		}
		else {
			switch(field->type) {
			case TSFILE_FIELD_INT:
				json_add_string(field_node, JSON_STR("type"), JSON_STR("int"));
				break;
			case TSFILE_FIELD_FLOAT:
				json_add_string(field_node, JSON_STR("type"), JSON_STR("float"));
				break;
			case TSFILE_FIELD_STRING:
				json_add_string(field_node, JSON_STR("type"), JSON_STR("str"));
				break;
			case TSFILE_FIELD_START_TIME:
				json_add_string(field_node, JSON_STR("type"), JSON_STR("start_time"));
				break;
			case TSFILE_FIELD_END_TIME:
				json_add_string(field_node, JSON_STR("type"), JSON_STR("end_time"));
				break;
			}

			json_add_integer(field_node, JSON_STR("size"), field->size);
		}

		json_add_integer(field_node, JSON_STR("offset"), field->offset);

		json_add_node(fields, json_str_create(field->name), field_node);
	}

	return node;
}

