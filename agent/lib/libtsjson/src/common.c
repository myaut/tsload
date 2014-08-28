/*
 * common.c
 *
 *  Created on: Apr 25, 2014
 *      Author: myaut
 */

#include <mempool.h>
#include <threads.h>

#include <json.h>
#include <jsonimpl.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

mp_cache_t		json_node_mp;
mp_cache_t		json_buffer_mp;

json_buffer_t* json_buf_create(char* data, size_t sz, boolean_t reuse) {
	json_buffer_t* buf;
	char* buffer;

	if(reuse) {
		buffer = data;
	}
	else {
		buffer = mp_malloc(sz);
		if(!buffer)
			return NULL;

		memcpy(buffer, data, sz);
	}

	buf = mp_cache_alloc(&json_buffer_mp);
	buf->buffer = buffer;
	buf->size = sz;
	buf->ref_count = (atomic_t) 0l;

	return buf;
}

json_buffer_t* json_buf_from_file(const char* path) {
	FILE* file = fopen(path, "r");

	size_t filesize;
	size_t result;
	char* data;

	/* TODO: mmapped buffers */

	if(file == NULL) {
		json_set_error_str(errno, "Failed to open JSON file '%s'", path);
		return NULL;
	}

	/* Read schema into array */
	fseek(file, 0, SEEK_END);
	filesize = ftell(file);
	fseek(file, 0, SEEK_SET);

	data = mp_malloc(filesize  + 1);
	result = fread(data, 1, filesize, file);
	data[filesize] = '\0';

	fclose(file);

	if(result < filesize) {
		json_set_error_str(errno, "Failed to read JSON file '%s'", path);
		mp_free(data);
		return NULL;
	}

	return json_buf_create(data, filesize, B_TRUE);
}

void json_buf_free(json_buffer_t* buf) {
	mp_free(buf->buffer);
	mp_cache_free(&json_buffer_mp, buf);
}

/**
 * Create internal JSON string from str
 */
json_str_t json_str_create(const char* str) {
	size_t sz = strlen(str);
	char* json_str = mp_malloc(sz + 1);

	if(!json_str)
		return NULL;

	*json_str++ = JSON_STR_DYNAMIC;

	strncpy(json_str, str, sz);
	return (json_str_t) json_str;
}

/**
 * Create internal JSON string from part of buffer
 *
 * @param buf Pointer to JSON buffer
 * @param from Index of first character
 * @param to Index to last character
 */
json_str_t json_str_reference(json_buffer_t* buf, int from, int to) {
	char* json_buf = buf->buffer;

	json_buf_hold(buf);

	json_buf[from - 1] = JSON_STR_REFERENCE;
	json_buf[to] = '\0';

	return (json_str_t) (json_buf + from);
}

void json_str_free(json_str_t json_str, json_buffer_t* buf) {
	const char* str = (const char*) json_str;
	char tag = str[-1];

	if(tag == JSON_STR_DYNAMIC) {
		mp_free(str - 1);
		return;
	}

	if(tag == JSON_STR_REFERENCE) {
		json_buf_rele(buf);
		return;
	}

	assert(tag == JSON_STR_CONST);
}

/**
 * Create a raw JSON node object.
 *
 * @param buf Referenced buffer for parsed nodes.  \
 * 			  Should be set to NULL for nodes created by user
 *
 * @note Do not use this function directly.
 */
json_node_t* json_node_create(json_buffer_t* buf, json_type_t type) {
	json_node_t* node = mp_cache_alloc(&json_node_mp);

	if(!node)
		return NULL;

	node->jn_parent = NULL;

	node->jn_buf	= buf;
	node->jn_name	= NULL;

	node->jn_type 	= type;

	node->jn_children_count = 0;
	list_head_init(&node->jn_child_head, "jn_child_head");
	list_node_init(&node->jn_child_node);

	return node;
}

void json_node_destroy(json_node_t* node) {
	json_node_t* child;
	json_node_t* next;

	list_del(&node->jn_child_node);

	list_for_each_entry_safe(json_node_t, child, next,
							 &node->jn_child_head, jn_child_node) {
		json_node_destroy(child);
	}

	if(node->jn_name) {
		json_str_free(node->jn_name, node->jn_buf);
	}

	if(node->jn_type == JSON_STRING) {
		json_str_free(node->jn_data.s, node->jn_buf);
	}

	mp_cache_free(&json_node_mp, node);
}

int json_init(void) {
	mp_cache_init(&json_node_mp, json_node_t);
	mp_cache_init(&json_buffer_mp, json_buffer_t);

	return json_init_errors();
}

void json_fini(void) {
	json_destroy_errors();

	mp_cache_destroy(&json_node_mp);
	mp_cache_destroy(&json_buffer_mp);
}
