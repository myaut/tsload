/*
 * tsfile.c
 *
 *  Created on: Jan 7, 2014
 *      Author: myaut
 */

#include <tsfile.h>
#include <plat/posixdecl.h>
#include <mempool.h>
#include <threads.h>

#include <assert.h>

mp_cache_t	tsfile_cache;

tsfile_error_msg_func tsfile_error_msg = NULL;
int tsfile_errno;

#define TSFILE_SB_GET_COUNT(header, sbi)					\
			((header)->sb[(sbi)].count)
#define TSFILE_SB_SET_COUNT(header, sbi, new_count)			\
			((header)->sb[(sbi)].count = new_count);		\
			((header)->sb[(sbi)].time = tm_get_time())

boolean_t tsfile_sync_mode = B_FALSE;

/**
 * TimeSeries File Format Library
 *
 * Sinse TSLoad operates with data sequentally: steps, requests are sequences, no need
 * to use conventional databases (both relational or key-value) to keep it's data.
 * Instead, it uses TSF format, and libtsfile provides API to work with it.
 *
 * API consists of two levels:
 * - Low level API, which operates with RAW schemas and objects
 * - High level API, which uses JSON representation
 * and have three primary calls: get count of entries, get some entries and add entries
 * to the end of tsfile.
 *
 * API is MT-safe, but doesn't lock files, so they may be opened from different processes.
 *
 * Since tsfile reuses JSON nodes it allocates, do not throw them away using json_delete,
 * use json_tsfile_put()/json_tsfile_put_array() functions instead.
 *
 * Library may be used in both agent and standalone context, but you need to override
 * tsfile_error_msg function pointer.
 *
 * NOTE: superblocks are located in same disk block, so multiple copies are useless */

static tsfile_t* tsfile_open_file(const char* filename, boolean_t create) {
	tsfile_t* file = NULL;
	int fd;
	size_t filename_len;
	int flags = O_RDWR;

	if(tsfile_sync_mode)
		flags |= O_SYNC;

	if(create) {
		fd = open(filename, flags | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
	}
	else {
		fd = open(filename, flags);
	}

	if(fd == -1) {
		tsfile_error_msg(TSE_INTERNAL_ERROR, "Failed to open tsfile '%s'!", filename);

		return NULL;
	}

	file = mp_cache_alloc(&tsfile_cache);

	file->fd = fd;

	filename_len = strlen(filename) + 1;
	file->filename = mp_malloc(filename_len);
	strncpy(file->filename, filename, filename_len);

	if(create) {
		file->size = 0;
	}
	else {
		file->size = lseek(fd, 0, SEEK_END);
		lseek(fd, 0, SEEK_SET);
	}

	file->header = NULL;

	return file;
}

static void tsfile_close_file(tsfile_t* file) {
	if(file->header)
		mp_free(file->header);

	mp_free(file->filename);
	close(file->fd);

	mp_cache_free(&tsfile_cache, file);
}

static boolean_t tsfile_check_schema(tsfile_schema_t* schema) {
	int fi;

	if(schema->hdr.count > MAXFIELDCOUNT) {
		tsfile_error_msg(TSE_INTERNAL_ERROR,
						 "Invalid schema- too many fields");
		return B_FALSE;
	}

	for(fi = 0; fi < schema->hdr.count; ++fi) {
		switch(schema->fields[fi].type) {
		case TSFILE_FIELD_BOOLEAN:
		case TSFILE_FIELD_STRING:
			break;
		case TSFILE_FIELD_INT:
		{
			switch(schema->fields[fi].size) {
			case 1: case 2:
			case 4: case 8:
				break;
			default:
				tsfile_error_msg(TSE_INTERNAL_ERROR,
								 "Invalid schema field #%d - wrong size of integer", fi);
				return B_FALSE;
			}
		}
		break;
		case TSFILE_FIELD_FLOAT:
		{
			switch(schema->fields[fi].size) {
			case sizeof(float):
			case sizeof(double):
				break;
			default:
				tsfile_error_msg(TSE_INTERNAL_ERROR,
								 "Invalid schema field #%d - wrong size of float", fi);
				return B_FALSE;
			}
		}
		break;
		default:
			tsfile_error_msg(TSE_INTERNAL_ERROR,
							 "Invalid schema field #%d - wrong type", fi);
			return B_FALSE;
		}
	}

	return B_TRUE;
}

static boolean_t tsfile_validate_schema(tsfile_header_t* header, tsfile_schema_t* schema2) {
	int fi;

	tsfile_schema_t* schema1 = &header->schema;
	tsfile_field_t *f1, *f2;

	if(schema1->hdr.count 	   != schema2->hdr.count ||
	   schema1->hdr.entry_size != schema2->hdr.entry_size) {
			tsfile_error_msg(TSE_INTERNAL_ERROR,
							 "Different schema headers: hdr: %d,%d schema: %d,%d",
							 schema1->hdr.count, schema1->hdr.entry_size,
							 schema2->hdr.count, schema2->hdr.entry_size);
			return B_FALSE;
	}

	for(fi = 0; fi < schema1->hdr.count; ++fi) {
		f1 = &schema1->fields[fi];
		f2 = &schema2->fields[fi];

		if(f1->type != f2->type) {
			tsfile_error_msg(TSE_INTERNAL_ERROR,
							 "Different schema field #%d - type", fi);
			return B_FALSE;
		}

		if(strncmp(f1->name, f2->name, MAXFIELDLEN) != 0) {
			tsfile_error_msg(TSE_INTERNAL_ERROR,
							 "Different schema field #%d - name", fi);
			return B_FALSE;
		}

		if(f1->type != TSFILE_FIELD_BOOLEAN) {
			if(f1->size != f2->size) {
				tsfile_error_msg(TSE_INTERNAL_ERROR,
								 "Different schema field #%d - size", fi);
				return B_FALSE;
			}
		}

	}

	return B_TRUE;
}

tsfile_t* tsfile_create(const char* filename, tsfile_schema_t* schema) {
	tsfile_t* file = NULL;
	tsfile_header_t* header = NULL;

	size_t schema_size = sizeof(tsfile_shdr_t) +
			  	 	 	 schema->hdr.count * sizeof(tsfile_field_t);

	if(!tsfile_check_schema(schema)) {
		return NULL;
	}

	file = tsfile_open_file(filename, B_TRUE);
	if(file == NULL)
		return NULL;

	/* Initialize header */
	header = mp_malloc(TSFILE_HEADER_SIZE);

	memcpy(&header->magic, TSFILE_MAGIC, TSFILE_MAGIC_LEN);
	header->version = TSFILE_VERSION;

	memset(&header->sb[1], 0, sizeof(tsfile_sb_t) * (SBCOUNT - 1));
	TSFILE_SB_SET_COUNT(header, 0, 0);

	memset(&header->schema, 0, sizeof(tsfile_schema_t));
	memcpy(&header->schema, schema, schema_size);

	memset(header + 1, 0, TSFILE_HEADER_SIZE - sizeof(tsfile_header_t));

	if(write(file->fd, header, TSFILE_HEADER_SIZE) < TSFILE_HEADER_SIZE) {
		tsfile_error_msg(TSE_INTERNAL_ERROR, "Failed to write header of '%s'", filename);

		mp_free(header);
		tsfile_close_file(file);

		return NULL;
	}

	file->header = header;
	file->cur_sb = 0ul;

	mutex_init(&file->mutex, "tsfile-%x", file);

	tsfile_init_nodes(file);

	return file;
}

tsfile_t* tsfile_open(const char* filename, tsfile_schema_t* schema) {
	tsfile_t* file = tsfile_open_file(filename, B_FALSE);
	tsfile_header_t* header = NULL;

	ts_time_t cur_time = tm_get_time();
	int sbi, cur_sb;

	if(file == NULL)
		return NULL;

	header = mp_malloc(TSFILE_HEADER_SIZE);

	if(read(file->fd, header, TSFILE_HEADER_SIZE) < TSFILE_HEADER_SIZE) {
		tsfile_error_msg(TSE_INTERNAL_ERROR, "Failed to read header of '%s'", filename);
		goto bad_file;
	}

	if(memcmp(&header->magic, TSFILE_MAGIC, TSFILE_MAGIC_LEN) != 0 ||
	   header->version != TSFILE_VERSION) {
		tsfile_error_msg(TSE_INTERNAL_ERROR, "Invalid tsfile magic/version '%s'", filename);
		goto bad_file;
	}

	if(!tsfile_validate_schema(header, schema)) {
		tsfile_error_msg(TSE_INTERNAL_ERROR, "Invalid schema in file '%s'", filename);
		goto bad_file;
	}

	/* Find actual superblock (it has maximum time label) */
	cur_sb = -1;
	for(sbi = 0; sbi < SBCOUNT; ++sbi) {
		if(header->sb[sbi].time > 0) {
			if(cur_sb == -1 ||
			   header->sb[sbi].time > header->sb[cur_sb].time) {
					cur_sb = sbi;
			}
		}
	}

	if(cur_sb < 0) {
		tsfile_error_msg(TSE_INTERNAL_ERROR, "Failed to find superblock in '%s'", filename);
		goto bad_file;
	}

	if(header->sb[cur_sb].time > cur_time) {
		tsfile_error_msg(TSE_INTERNAL_ERROR, "Superblock timestamp %lld is in future");
	}

	file->cur_sb = cur_sb;
	file->header = header;

	mutex_init(&file->mutex, "tsfile-%x", file);

	tsfile_init_nodes(file);

	return file;

bad_file:
	mp_free(header);
	tsfile_close_file(file);

	return NULL;
}

void tsfile_close(tsfile_t* file) {
	mutex_destroy(&file->mutex);
	tsfile_destroy_nodes(file);
	tsfile_close_file(file);
}

int tsfile_add(tsfile_t* file, void* entries, unsigned count) {
	uint32_t cur_count;
	unsigned long entry_size = file->header->schema.hdr.entry_size;
	unsigned long size = count * entry_size;
	off_t end;

	tsfile_errno = TSFILE_OK;

	mutex_lock(&file->mutex);

	/* Calculate counts and offsets */
	cur_count = TSFILE_SB_GET_COUNT(file->header, file->cur_sb);
	end = TSFILE_HEADER_SIZE + cur_count * entry_size;

	/* Write to the end of file */
	if(lseek(file->fd, end, SEEK_SET) == ((off_t)-1) ||
	   write(file->fd, entries, size) < size) {
		tsfile_errno = TSFILE_DATA_FAIL;
		goto end;
	}

	file->size += size;

	/* Update superblock */
	file->cur_sb = (file->cur_sb + 1) & SBMASK;
	TSFILE_SB_SET_COUNT(file->header, file->cur_sb, cur_count + count);

	/* Rewrite header (only first 512 bytes actually) */
	if(lseek(file->fd, 0, SEEK_SET) == ((off_t)-1) ||
	   write(file->fd, file->header,
			 TSFILE_SB_WRITE_LEN) < TSFILE_SB_WRITE_LEN) {
		tsfile_errno = TSFILE_SB_FAIL;
	}

end:
	mutex_unlock(&file->mutex);

	return tsfile_errno;
}

uint32_t tsfile_get_count(tsfile_t* file) {
	uint32_t cur_count;

	mutex_lock(&file->mutex);
	cur_count = TSFILE_SB_GET_COUNT(file->header, file->cur_sb);
	mutex_unlock(&file->mutex);

	return cur_count;
}

int tsfile_get_entries(tsfile_t* file, void* entries, unsigned start, unsigned end) {
	uint32_t cur_count;

	unsigned long entry_size = file->header->schema.hdr.entry_size;
	off_t off = TSFILE_HEADER_SIZE + start * entry_size;
	unsigned long size = (end - start) * entry_size;

	if(start >= end) {
		tsfile_errno = TSFILE_INVAL_RANGE;
		return tsfile_errno;
	}

	tsfile_errno = TSFILE_OK;

	mutex_lock(&file->mutex);
	cur_count = TSFILE_SB_GET_COUNT(file->header, file->cur_sb);

	if(start > cur_count || end > cur_count) {
		mutex_unlock(&file->mutex);
		tsfile_errno = TSFILE_INVAL_RANGE;
		return tsfile_errno;
	}

	if(lseek(file->fd, off, SEEK_SET) == ((off_t)-1) ||
	   read(file->fd, entries, size) < size) {
		tsfile_errno = TSFILE_DATA_FAIL;
	}

	mutex_unlock(&file->mutex);

	return tsfile_errno;
}

JSONNODE* json_tsfile_get(tsfile_t* file, unsigned number) {
	JSONNODE** nodes = NULL;
	unsigned long entry_size = file->header->schema.hdr.entry_size;
	void* entry = mp_malloc(entry_size);

	JSONNODE* node = NULL;

	if(tsfile_get_entries(file, entry, number, number + 1) != TSFILE_OK) {
		goto end;
	}

	nodes = tsfile_get_nodes(file, 1);
	node = nodes[0];

	tsfile_fill_node(file, node, entry);

end:
	if(nodes)
		mp_free(nodes);
	mp_free(entry);

	return node;
}

void json_tsfile_put(tsfile_t* file, JSONNODE* node) {
	tsfile_put_node(file, node);
}

int json_tsfile_add(tsfile_t* file, JSONNODE* node) {
	unsigned long entry_size = file->header->schema.hdr.entry_size;
	void* entry = mp_malloc(entry_size);

	int ret;

	tsfile_fill_entry(file, node, entry);
	ret = tsfile_add(file, entry, 1);

	mp_free(entry);
	return ret;
}

JSONNODE* json_tsfile_get_array(tsfile_t* file, unsigned start, unsigned end) {
	JSONNODE** nodes = NULL;
	JSONNODE* node_array = NULL;
	unsigned long entry_size = file->header->schema.hdr.entry_size;
	void* entries = NULL;
	void* entry;
	int count = end - start;
	int ni;

	if(count <= 0) {
		tsfile_errno = TSFILE_INVAL_RANGE;
		return NULL;
	}

	entries = mp_malloc(entry_size * count);
	if(tsfile_get_entries(file, entries, start, end) != TSFILE_OK) {
		goto end;
	}

	nodes = tsfile_get_nodes(file, count);
	node_array = json_new(JSON_ARRAY);

	for(ni = 0; ni < count; ++ni) {
		entry = ((char*) entries) + entry_size * ni;
		tsfile_fill_node(file, nodes[ni], entry);

		json_push_back(node_array, nodes[ni]);
	}

end:
	if(nodes)
		mp_free(nodes);
	mp_free(entries);
	return node_array;
}

void json_tsfile_put_array(tsfile_t* file, JSONNODE* node_array) {
	int size = json_size(node_array);
	int i;
	JSONNODE* node;

	for(i = 0; i < size; ++i) {
		node = json_pop_back_at(node_array, 0);

		if(!tsfile_put_node(file, node)) {
			/* Filled "death row" array. Abandon other nodes
			 * They will be deallocated in json_delete() call below */
			break;
		}
	}

	json_delete(node_array);
}

int json_tsfile_add_array(tsfile_t* file, JSONNODE* node_array) {
	JSONNODE_ITERATOR i_end, i_node;
	unsigned long entry_size = file->header->schema.hdr.entry_size;
	void* entries = NULL;
	void* entry;
	int count = json_size(node_array);
	int ni = 0;
	int ret;

	entries = mp_malloc(entry_size * count);

	i_node = json_begin(node_array);
	i_end = json_end(node_array);

	while(i_node != i_end) {
		entry = ((char*) entries) + entry_size * ni;
		tsfile_fill_entry(file, *i_node, entry);

		++i_node;
		++ni;
	}

	ret = tsfile_add(file, entries, count);

	mp_free(entries);
	return ret;
}

int tsfile_init(void) {
	mp_cache_init(&tsfile_cache, tsfile_t);

	return 0;
}

void tsfile_fini(void) {
	mp_cache_destroy(&tsfile_cache);
}
