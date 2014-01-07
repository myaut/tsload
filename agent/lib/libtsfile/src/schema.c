/*
 * schema.c
 *
 *  Created on: Jan 7, 2014
 *      Author: myaut
 */

#include <tsfile.h>
#include <mempool.h>

#include <libjson.h>

#include <assert.h>

#define PARSE_SCHEMA_PARAM(root, i_node, name, type) 						\
	i_node = json_find(root, name);											\
	if(i_node == i_end || json_type(*i_node) != type) {						\
		tsfile_error_msg(TSE_MESSAGE_FORMAT, 								\
						 "Missing or invalid param '%s' in schema", name);	\
		return NULL;														\
	}

#define PARSE_SCHEMA_FIELD(root, i_node, name, type, error) 			\
	i_node = json_find(root, name);										\
	if(i_node == i_end || json_type(*i_node) != type) {					\
		return error; 													\
	}

static int schema_parse_field(tsfile_field_t* field, JSONNODE* node, ptrdiff_t offset);

tsfile_schema_t* tsfile_schema_parse(JSONNODE* root, boolean_t auto_offset) {
	JSONNODE_ITERATOR i_size, i_fields, i_end;
	JSONNODE_ITERATOR i_fields_end, i_field;

	tsfile_schema_t* schema = NULL;
	tsfile_field_t* field;
	size_t schema_size;
	ptrdiff_t offset = (ptrdiff_t) -1;
	int field_count, fi;
	int err;

	i_end = json_end(root);

	PARSE_SCHEMA_PARAM(root, i_size, "entry_size", JSON_NUMBER);
	PARSE_SCHEMA_PARAM(root, i_fields, "fields", JSON_NODE);

	field_count = json_size(*i_fields);
	schema_size = sizeof(tsfile_shdr_t) +
			 	  field_count * sizeof(tsfile_field_t);

	if(field_count > MAXFIELDCOUNT) {
		tsfile_error_msg(TSE_MESSAGE_FORMAT, "Too many fields in schema");
		return NULL;
	}

	schema = mp_malloc(schema_size);

	schema->hdr.entry_size = json_as_int(*i_size);
	schema->hdr.count = field_count;

	i_field = json_begin(*i_fields);
	i_fields_end = json_end(*i_fields);

	if(auto_offset) {
		offset = 0;
	}

	for(fi = 0; fi < field_count; ++fi) {
		assert(i_field != i_fields_end);

		field = &schema->fields[fi];

		err = schema_parse_field(field, *i_field, offset);
		if(err != SCHEMA_FIELD_OK) {
			tsfile_error_msg(TSE_MESSAGE_FORMAT,
							 "Failed to parse field '%s': error %d", field->name, err);

			mp_free(schema);
			return NULL;
		}

		if(auto_offset) {
			offset += field->size;
		}

		++i_field;
	}

	return schema;
}

static int schema_parse_field(tsfile_field_t* field, JSONNODE* node, ptrdiff_t offset) {
	JSONNODE_ITERATOR i_type, i_size, i_offset, i_end;
	char* field_name;
	char* field_type;
	boolean_t need_size = B_TRUE;

	field_name = json_name(node);
	strncpy(field->name, field_name, MAXFIELDLEN);
	json_free(field_name);

	i_end = json_end(node);

	/* Parse field type */
	PARSE_SCHEMA_FIELD(node, i_type, "type", JSON_STRING,
							SCHEMA_FIELD_MISSING_TYPE);

	field_type = json_as_string(*i_type);
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
	else if(strcmp(field_type, "string") == 0) {
		field->type = TSFILE_FIELD_STRING;
	}
	else {
		json_free(field_type);
		return SCHEMA_FIELD_INVALID_TYPE;
	}

	json_free(field_type);

	/* Parse field offset */
	if(offset == ((ptrdiff_t)-1)) {
		PARSE_SCHEMA_FIELD(node, i_offset, "offset", JSON_NUMBER,
								SCHEMA_FIELD_MISSING_OFF);
		field->offset = json_as_int(*i_offset);
	}

	/* Parse field size */
	if(need_size) {
		PARSE_SCHEMA_FIELD(node, i_size, "size", JSON_NUMBER,
								SCHEMA_FIELD_MISSING_SIZE);
		field->size = json_as_int(*i_size);
	}

	return SCHEMA_FIELD_OK;
}
