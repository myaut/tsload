/*
 * nodes.c
 *
 *  Created on: Jan 7, 2014
 *      Author: myaut
 */

#include <tsfile.h>
#include <mempool.h>

#include <libjson.h>

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

int	tsfile_nodes_count = 40;

#define	FIELD_GET_VALUE(type, value)			*((type*) value)
#define	FIELD_PUT_VALUE(type, value, new_value)	*((type*) value) = (type) new_value

void tsfile_init_nodes(tsfile_t* file) {
	file->node_count = 0;
	file->node_first = 0;

	file->node_cache = mp_malloc(sizeof(JSONNODE*) * tsfile_nodes_count);

	mutex_init(&file->node_mutex, "tsfile-n-%x", file);
}

void tsfile_destroy_nodes(tsfile_t* file) {
	int i, j;

	for(i = 0, j = file->node_first;
		i < file->node_count;
		++i, ++j) {
			if(j == tsfile_nodes_count)
				j = 0;

			json_delete(file->node_cache[j]);
	}

	mutex_destroy(&file->node_mutex);
}

JSONNODE* tsfile_create_node(tsfile_t* file) {
	JSONNODE* node = json_new(JSON_NODE);
	JSONNODE* field;
	tsfile_schema_t* schema = &file->header->schema;
	int fi;

	for(fi = 0; fi < schema->hdr.count; ++fi) {
		switch(schema->fields[fi].type) {
		case TSFILE_FIELD_BOOLEAN:
			field = json_new(JSON_BOOL);
		break;
		case TSFILE_FIELD_INT:
		case TSFILE_FIELD_FLOAT:
			field = json_new(JSON_NUMBER);
		break;
		case TSFILE_FIELD_STRING:
			field = json_new(JSON_STRING);
		break;
		}

		json_set_name(field, schema->fields[fi].name);
		json_push_back(node, field);
	}

	return node;
}

JSONNODE** tsfile_get_nodes(tsfile_t* file, int count) {
	JSONNODE** nodes = mp_malloc(sizeof(JSONNODE*) * count);
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

boolean_t tsfile_put_node(tsfile_t* file, JSONNODE* node) {
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
		json_delete(node);
	}

	return cached;
}

void tsfile_fill_node(tsfile_t* file, JSONNODE* node, void* entry) {
	/* TODO: endianess conversion */
	JSONNODE_ITERATOR i_field, i_end;

	tsfile_schema_t* schema = &file->header->schema;
	int fi;

	void* value;

	i_field = json_begin(node);
	i_end = json_end(node);

	for(fi = 0; fi < schema->hdr.count; ++fi) {
		value = ((char*) entry) + schema->fields[fi].offset;
		assert(i_field != i_end);

		switch(schema->fields[fi].type) {
		case TSFILE_FIELD_BOOLEAN:
			json_set_b(*i_field, FIELD_GET_VALUE(boolean_t, value));
		break;
		case TSFILE_FIELD_INT:
		{
			switch(schema->fields[fi].size) {
			case 1:
				json_set_i(*i_field, FIELD_GET_VALUE(uint8_t, value));
			break;
			case 2:
				json_set_i(*i_field, FIELD_GET_VALUE(uint16_t, value));
			break;
			case 4:
				json_set_i(*i_field, FIELD_GET_VALUE(uint32_t, value));
			break;
			case 8:
				json_set_i(*i_field, FIELD_GET_VALUE(uint64_t, value));
			break;
			}
		}
		break;
		case TSFILE_FIELD_FLOAT:
		{
			switch(schema->fields[fi].size) {
			case sizeof(float):
				json_set_f(*i_field, FIELD_GET_VALUE(float, value));
			break;
			case sizeof(double):
				json_set_f(*i_field, FIELD_GET_VALUE(double, value));
			break;
			}
		}
		break;
		case TSFILE_FIELD_STRING:
			json_set_a(*i_field, (char*) value);
		break;
		}

		++i_field;
	}
}

int tsfile_fill_entry(tsfile_t* file, JSONNODE* node, void* entry) {
	/* TODO: endianess conversion */
	JSONNODE_ITERATOR i_field, i_end;

	tsfile_schema_t* schema = &file->header->schema;
	tsfile_field_t* field;
	int fi;

	void* value;
	char* str;

	i_end = json_end(node);

	for(fi = 0; fi < schema->hdr.count; ++fi) {
		field = &schema->fields[fi];
		value = ((char*) entry) + field->offset;

		i_field = json_find(node, field->name);
		if(i_field == i_end) {
			return -1;
		}

		switch(schema->fields[fi].type) {
		case TSFILE_FIELD_BOOLEAN:
			FIELD_PUT_VALUE(boolean_t, value, json_as_bool(*i_field));
		break;
		case TSFILE_FIELD_INT:
		{
			switch(schema->fields[fi].size) {
			case 1:
				FIELD_PUT_VALUE(uint8_t, value, json_as_int(*i_field));
			break;
			case 2:
				FIELD_PUT_VALUE(uint16_t, value, json_as_int(*i_field));
			break;
			case 4:
				FIELD_PUT_VALUE(uint32_t, value, json_as_int(*i_field));
			break;
			case 8:
				FIELD_PUT_VALUE(uint64_t, value, json_as_int(*i_field));
			break;
			}
		}
		break;
		case TSFILE_FIELD_FLOAT:
		{
			switch(schema->fields[fi].size) {
			case sizeof(float):
				FIELD_PUT_VALUE(float, value, json_as_float(*i_field));
			break;
			case sizeof(double):
				FIELD_PUT_VALUE(double, value, json_as_float(*i_field));
			break;
			}
		}
		break;
		case TSFILE_FIELD_STRING:
			str = json_as_string(*i_field);
			strncpy(value, str, field->size);
			json_free(str);
		break;
		}
	}

	return 0;
}
