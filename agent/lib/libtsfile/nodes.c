
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
#include <tsload/field.h>

#include <tsload/json/json.h>

#include <tsfile.h>

#include <string.h>
#include <assert.h>


/**
 * nodes.c - serialization/deserialization engine
 *
 * Since tsfile_create_node() is not cheap operation because each time
 * we need to construct std::string for each field name, we keep "death row"
 * on tsfile object.
 *
 * When TSDB gets nodes from file, tsfile_get_nodes() searches "death row" first
 * and takes nodes from there, then allocates new one. After TSDB uses them, it should
 * put them back through tsfile_put_node(), which returns nodes to "death row" if
 * possible. These functions are internal and called from json_tsfile_* routines.
 *
 * tsfile_fill_node() deserializes entry, while tsfile_fill_entry() serializes it
 */

DECLARE_FIELD_FUNCTION_BYTE(uint8_t);
DECLARE_FIELD_FUNCTIONS(boolean_t);
DECLARE_FIELD_FUNCTIONS(uint16_t);
DECLARE_FIELD_FUNCTIONS(uint32_t);
DECLARE_FIELD_FUNCTIONS(uint64_t);
DECLARE_FIELD_FUNCTIONS(float);
DECLARE_FIELD_FUNCTIONS(double);
DECLARE_FIELD_FUNCTIONS(ts_time_t);

/**
 * Count of nodes on death row of tsfile. Reduces number of allocation/initializations.
 */
int	tsfile_nodes_count = 40;

void tsfile_init_nodes(tsfile_t* file) {
	file->node_count = 0;
	file->node_first = 0;

	file->node_cache = mp_malloc(sizeof(json_node_t*) * tsfile_nodes_count);

	mutex_init(&file->node_mutex, "tsfile-n-%p", file);
}

void tsfile_destroy_nodes(tsfile_t* file) {
	int i, j;

	for(i = 0, j = file->node_first;
		i < file->node_count;
		++i, ++j) {
			if(j == tsfile_nodes_count)
				j = 0;

			json_node_destroy(file->node_cache[j]);
	}

	mp_free(file->node_cache);

	mutex_destroy(&file->node_mutex);
}

json_node_t* tsfile_create_node(tsfile_t* file) {
	json_node_t* node = json_new_node(NULL);
	json_node_t* field;
	tsfile_schema_t* schema = &file->header->schema;
	int fi;

	for(fi = 0; fi < schema->hdr.count; ++fi) {
		switch(schema->fields[fi].type) {
		case TSFILE_FIELD_BOOLEAN:
			field = json_new_boolean(B_FALSE);
		break;
		case TSFILE_FIELD_INT:
		case TSFILE_FIELD_START_TIME:
		case TSFILE_FIELD_END_TIME:
			field = json_new_integer(-1);
			break;
		case TSFILE_FIELD_FLOAT:
			field = json_new_double(0.0);
		break;
		case TSFILE_FIELD_STRING:
			field = json_new_string(JSON_STR(""));
		break;
		}

		json_add_node(node, json_str_create(schema->fields[fi].name), field);
	}

	return node;
}

json_node_t** tsfile_get_nodes(tsfile_t* file, int count) {
	json_node_t** nodes = mp_malloc(sizeof(json_node_t*) * count);
	int i;

	/* Try to claim nodes from node_cache */
	mutex_lock(&file->node_mutex);

	for(i = 0; (file->node_count > 0) && (i < count); ++i) {
			if(file->node_first == tsfile_nodes_count)
				file->node_first = 0;

			nodes[i] = file->node_cache[file->node_first];
			file->node_cache[file->node_first] = NULL;

			--file->node_count;
			++file->node_first;
	}

	mutex_unlock(&file->node_mutex);

	/* node_cache exhausted - allocate new ones */
	for( ; i < count; ++i ) {
		nodes[i] = tsfile_create_node(file);
	}

	return nodes;
}

boolean_t tsfile_put_node(tsfile_t* file, json_node_t* node) {
	boolean_t cached = B_FALSE;

	mutex_lock(&file->node_mutex);

	if(file->node_count < tsfile_nodes_count) {
		--file->node_first;
		++file->node_count;

		if(file->node_first < 0) {
			file->node_first = tsfile_nodes_count - 1;
		}

		file->node_cache[file->node_first] = node;
		cached = B_TRUE;
	}

	mutex_unlock(&file->node_mutex);

	if(!cached) {
		json_node_destroy(node);
	}

	return cached;
}

void tsfile_fill_node(tsfile_t* file, json_node_t* node, void* entry) {
	/* TODO: endianess conversion */
	int id = 0;
	json_node_t* field = json_first(node, &id);

	tsfile_schema_t* schema = &file->header->schema;
	int fi;

	void* value;

	for(fi = 0; fi < schema->hdr.count; ++fi) {
		value = ((char*) entry) + schema->fields[fi].offset;

		assert(!json_is_end(node, field, &id));

		switch(schema->fields[fi].type) {
		case TSFILE_FIELD_BOOLEAN:
			json_set_boolean(field, FIELD_GET_VALUE(boolean_t, value));
		break;
		case TSFILE_FIELD_INT:
		{
			switch(schema->fields[fi].size) {
			case 1:
				json_set_integer(field, FIELD_GET_VALUE(uint8_t, value));
			break;
			case 2:
				json_set_integer(field, FIELD_GET_VALUE(uint16_t, value));
			break;
			case 4:
				json_set_integer(field, FIELD_GET_VALUE(uint32_t, value));
			break;
			case 8:
				json_set_integer(field, FIELD_GET_VALUE(uint64_t, value));
			break;
			}
		}
		break;
		case TSFILE_FIELD_FLOAT:
		{
			switch(schema->fields[fi].size) {
			case sizeof(float):
				json_set_double(field, FIELD_GET_VALUE(float, value));
			break;
			case sizeof(double):
				json_set_double(field, FIELD_GET_VALUE(double, value));
			break;
			}
		}
		break;
		case TSFILE_FIELD_STRING:
			json_set_string(field, json_str_create(value));
			break;
		case TSFILE_FIELD_START_TIME:
		case TSFILE_FIELD_END_TIME:
			json_set_integer(field, FIELD_GET_VALUE(ts_time_t, value));
			break;
		}

		field = json_next(field, &id);
	}
}

int tsfile_fill_entry(tsfile_t* file, json_node_t* node, void* entry) {
	/* TODO: endianess conversion */
	tsfile_schema_t* schema = &file->header->schema;
	tsfile_field_t* field;
	int fi;

	void* value;

	json_node_t* j_field;

	for(fi = 0; fi < schema->hdr.count; ++fi) {
		field = &schema->fields[fi];
		value = ((char*) entry) + field->offset;

		j_field = json_find(node, field->name);
		if(j_field == NULL) {
			return -1;
		}

		switch(field->type) {
		case TSFILE_FIELD_BOOLEAN:
			FIELD_PUT_VALUE(boolean_t, value, json_as_boolean(j_field));
		break;
		case TSFILE_FIELD_INT:
		{
			switch(schema->fields[fi].size) {
			case 1:
				FIELD_PUT_VALUE(uint8_t, value, json_as_integer(j_field));
			break;
			case 2:
				FIELD_PUT_VALUE(uint16_t, value, json_as_integer(j_field));
			break;
			case 4:
				FIELD_PUT_VALUE(uint32_t, value, json_as_integer(j_field));
			break;
			case 8:
				FIELD_PUT_VALUE(uint64_t, value, json_as_integer(j_field));
			break;
			}
		}
		break;
		case TSFILE_FIELD_FLOAT:
		{
			switch(schema->fields[fi].size) {
			case sizeof(float):
				FIELD_PUT_VALUE(float, value, json_as_double(j_field));
			break;
			case sizeof(double):
				FIELD_PUT_VALUE(double, value, json_as_double(j_field));
			break;
			}
		}
		break;
		case TSFILE_FIELD_STRING:
			strncpy(value, json_as_string(j_field), field->size);
			break;
		case TSFILE_FIELD_START_TIME:
		case TSFILE_FIELD_END_TIME:
			FIELD_PUT_VALUE(ts_time_t, value, json_as_integer(j_field));
			break;
		}
	}

	return 0;
}

