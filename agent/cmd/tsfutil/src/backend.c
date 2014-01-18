/*
 * backend.c
 *
 *  Created on: Jan 8, 2014
 *      Author: myaut
 */

#define LOG_SOURCE "tsfutil"
#include <log.h>

#include <tsfutil.h>
#include <list.h>
#include <tsfile.h>
#include <csv.h>
#include <mempool.h>

#include <stdio.h>
#include <stdlib.h>

typedef char* (*json_write_func)(const JSONNODE* node);

size_t tsfutil_json_fragment  = 4096;
size_t tsfutil_json_min_alloc = 16384;

boolean_t tsfutil_json_print_one = B_FALSE;

const char* tsfile_error_str[] = {
	"OK",
	"Superblock failure",
	"Data failure",
	"Invalid range"
};

/*
 * JSON/JSON-RAW backends
 *
 * Actually implemented by libtsfile
 */

int tsfutil_json_set(const char* option) {
	if(strcmp(option, "one") == 0) {
		tsfutil_json_print_one = B_TRUE;
		return 1;
	}

	return 0;
}

int tsfutil_json_get_impl(FILE* file, tsfile_t* ts_file, int start, int end, json_write_func func) {
	char* str;
	JSONNODE* node;
	int ret = 0;

	if(tsfutil_json_print_one) {
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

	if(tsfutil_json_print_one) {
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

/*
 * CSV backend
 */

boolean_t csv_no_header = B_FALSE;
boolean_t csv_ext_header = B_FALSE;
boolean_t csv_all = B_FALSE;

char csv_header[CSV_HEADER_LENGTH];

unsigned csv_entries_cache_count = 8;

int tsfutil_csv_set(const char* option) {
	if(strncmp(option, "header=", 7) == 0) {
		strncpy(csv_header, option + 7, CSV_HEADER_LENGTH);
		csv_ext_header = B_TRUE;
		return 1;
	}
	else if(strncmp(option, "noheader", 7) == 0) {
		csv_no_header = B_TRUE;
		return 1;
	}
	else if(strncmp(option, "all", 7) == 0) {
		csv_all = B_TRUE;
		return 1;
	}

	return 0;
}

int tsfutil_csv_get(FILE* file, tsfile_t* ts_file, int start, int end) {
	csv_binding_t* bindings = NULL;
	int bcount;

	tsfile_schema_t* schema = &ts_file->header->schema;

	int ret;

	size_t entry_size = schema->hdr.entry_size;
	size_t write_len;
	void* entries = NULL;
	void* entry;
	int entry_idx = start, entry_count = 0, i = 0;

	/* Parse header */
	bindings = mp_malloc(schema->hdr.count * sizeof(csv_binding_t));

	ret = csv_generate_bindings((csv_ext_header)? csv_header : NULL, bindings, schema,
						  	    (csv_all)? CSV_HDR_GENERATE_ALL : CSV_HDR_GENERATE_OPT);

	if(ret < 1) {
		logmsg(LOG_CRIT, "CSV error %d", ret);
		goto end;
	}
	bcount = ret;

	if(!csv_no_header)
			csv_write_header(file, bindings, bcount);

	entries = mp_malloc(csv_entries_cache_count * entry_size);

	while(entry_idx < end) {
		entry_count = min(end - entry_idx, csv_entries_cache_count);

		ret = tsfile_get_entries(ts_file, entries, entry_idx, entry_idx + entry_count);
		if(ret != TSFILE_OK) {
			logmsg(LOG_CRIT, "TSFile error %d", ret);
			goto end;
		}

		for(i = 0; i < entry_count; ++i) {
			entry = entries + i * entry_size;
			csv_write_entry(file, bindings, bcount, entry);
		}

		entry_idx += entry_count;
	}

end:
	if(entries)
		mp_free(entries);

	mp_free(bindings);
	return ret;
}

int tsfutil_csv_add(FILE* file, tsfile_t* ts_file) {
	csv_binding_t* bindings = NULL; 			/* bindings is a mapping between index in
												   CSV file and index of field in schema */
	int bcount;
	list_head_t list;
	tsfile_schema_t* schema = &ts_file->header->schema;
	void* entry;

	int ret;

	char* line = NULL;
	char* read_ptr = NULL;
	size_t line_length, read_len;
	int lineno = (csv_no_header)? 1 : 2;

	/* Parse header */
	bindings = mp_malloc(schema->hdr.count * sizeof(csv_binding_t));
	list_head_init(&list, "csv_list");

	ret = csv_read_header((csv_no_header)? NULL : file,
						  (csv_ext_header)? csv_header : NULL,
						  bindings, schema);

	if(ret < 1) {
		logmsg(LOG_CRIT, "CSV error %d at header", ret);
		goto end;
	}
	bcount = ret;

	line_length = csv_max_line_length(bindings, bcount);
	line = mp_malloc(line_length);

	while(feof(file) == 0) {
		read_ptr = fgets(line, line_length, file);
		if(read_ptr == NULL)
			break;

		/* Cut out LF */
		read_len = strlen(line);
		if(line[read_len - 1] == '\n')
			line[--read_len] = '\0';

		entry = csv_entry_create(&list, schema->hdr.entry_size);
		ret = csv_read_entry(line, bindings, bcount, entry);

		if(ret != CSV_OK) {
			logmsg(LOG_CRIT, "CSV error %d at line %d", ret, lineno);
			goto end;
		}
	}

	ret = csv_entry_list_add(ts_file, &list);

end:
	if(line)
		mp_free(line);

	csv_entry_list_destroy(&list);

	mp_free(bindings);
	return ret;
}

tsfutil_backend_t json_backend = {
	tsfutil_json_set, tsfutil_json_get, tsfutil_json_add
};

tsfutil_backend_t jsonraw_backend = {
	tsfutil_json_set, tsfutil_jsonraw_get, tsfutil_json_add
};

tsfutil_backend_t csv_backend = {
	tsfutil_csv_set, tsfutil_csv_get, tsfutil_csv_add
};
