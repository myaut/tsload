
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

#include <tsload/list.h>
#include <tsload/mempool.h>

#include <tsfile.h>
#include <csv.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


size_t tsf_json_fragment  = 4096;
size_t tsf_json_min_alloc = 16384;

unsigned csv_entries_cache_count = 8;

struct tsf_json_backend {
	boolean_t json_print_one;
};

const char* tsfile_error_str[] = {
	"OK",
	"Superblock failure",
	"Data failure",
	"Invalid range"
};

extern int tsfile_errno;

/*
 * JSON/JSON-RAW backends
 */

int tsf_json_set(tsf_backend_t* backend, const char* option) {
	struct tsf_json_backend* json = (struct tsf_json_backend*) backend->private;

	if(strcmp(option, "one") == 0) {
		json->json_print_one = B_TRUE;
		return 1;
	}

	return 0;
}

int tsf_json_get_impl(tsf_backend_t* backend, int start, int end, boolean_t formatted) {
	json_node_t* node;
	int ret = 0;

	struct tsf_json_backend* json = (struct tsf_json_backend*) backend->private;

	if(json->json_print_one) {
		node = json_tsfile_get(backend->ts_file, start);
	}
	else {
		node = json_tsfile_get_array(backend->ts_file, start, end);
	}

	if(node == NULL) {
		logmsg(LOG_CRIT, "Failed to get JSON entries. Error: %s",
			   tsfile_error_str[-tsfile_errno]);
		return 1;
	}

	json_write_file(node, backend->file, formatted);

	if(json->json_print_one) {
		json_tsfile_put(backend->ts_file, node);
	}
	else {
		json_tsfile_put_array(backend->ts_file, node);
	}

	return ret;
}

int tsf_json_get(tsf_backend_t* backend, int start, int end) {
	return tsf_json_get_impl(backend, start, end, B_TRUE);
}

int tsf_jsonraw_get(tsf_backend_t* backend, int start, int end) {
	return tsf_json_get_impl(backend, start, end, B_FALSE);
}

int tsf_json_add(tsf_backend_t* backend) {
	size_t size = tsf_json_min_alloc;
	size_t length = 0;
	size_t read_len;
	char* str = mp_malloc(tsf_json_min_alloc);
	char* ptr = str;

	int err = 0;

	json_buffer_t* buf;
	json_node_t* node;

	size_t count;

	/* Read everything from file into str
	 * Couldn't memory map file because file may be stdin
	 *
	 * TODO: Rewrite this by allocating entire size of file */
	while(feof(backend->file) == 0) {
		read_len = fread(ptr, 1, tsf_json_fragment, backend->file);
		if(read_len == 0)
			break;

		length += read_len;
		ptr += read_len;

		if((size - length - 1) < tsf_json_fragment) {
			size += tsf_json_fragment;
			str = mp_realloc(str, size);
			ptr = str + length;

			if(str == NULL) {
				logmsg(LOG_CRIT, "Failed to reallocate %ld bytes", size);
				return 1;
			}
		}
	}

	str[length + 1] = '\0';

	json_errno_clear();

	buf = json_buf_create(str, length, B_TRUE);
	err = json_parse(buf, &node);

	if(err != 0) {
		logmsg(LOG_CRIT, "Couldn't parse JSON: %s", json_error_message());
		return 1;
	}

	count = json_size(node);

	if(json_type(node) == JSON_ARRAY) {
		err = json_tsfile_add_array(backend->ts_file, node);
	}
	else {
		err = json_tsfile_add(backend->ts_file, node);
	}

	json_node_destroy(node);

	if(err != TSFILE_OK) {
		logmsg(LOG_CRIT, "Failed to add JSON entries. Error: %s",
			   tsfile_error_str[-tsfile_errno]);
		return 1;
	}

	logmsg(LOG_INFO, "Added %" PRIsz " JSON entries to TSFile", count);

	return 0;
}

tsf_backend_class_t tsf_json_backend_class = {
	tsf_json_set, tsf_json_get, tsf_json_add
};

tsf_backend_class_t tsf_jsonraw_backend_class = {
	tsf_json_set, tsf_jsonraw_get, tsf_json_add
};

/*
 * CSV backend
 */

struct tsf_csv_backend {
	boolean_t csv_no_header;
	boolean_t csv_ext_header;
	boolean_t csv_all;

	int csv_separator;
	int csv_opt_separator;

	char csv_header[CSV_HEADER_LENGTH];
};

int tsf_csv_set(tsf_backend_t* backend, const char* option) {
	struct tsf_csv_backend* csv = (struct tsf_csv_backend*) backend->private;

	if(strncmp(option, "header=", 7) == 0) {
		strncpy(csv->csv_header, option + 7, CSV_HEADER_LENGTH);
		csv->csv_ext_header = B_TRUE;
		return 1;
	}
	else if(strncmp(option, "noheader", 8) == 0) {
		csv->csv_no_header = B_TRUE;
		return 1;
	}
	else if(strncmp(option, "all", 3) == 0) {
		csv->csv_all = B_TRUE;
		return 1;
	}
	else if(strncmp(option, "valsep=", 7) == 0) {
		if(strlen(option) != 8)
			return 0;

		csv->csv_separator = option[7];
		return 1;
	}
	else if(strncmp(option, "optsep=", 7) == 0) {
		if(strlen(option) != 8)
			return 0;

		csv->csv_opt_separator = option[7];
		return 1;
	}

	return 0;
}

int tsf_csv_get(tsf_backend_t* backend, int start, int end) {
	struct tsf_csv_backend* csv = (struct tsf_csv_backend*) backend->private;

	csv_binding_t* bindings = NULL;
	int bcount;

	tsfile_schema_t* schema = &backend->ts_file->header->schema;

	int ret;

	size_t entry_size = schema->hdr.entry_size;
	void* entries = NULL;
	void* entry;
	int entry_idx = start, entry_count = 0, i = 0;

	csv_chars_t chars;

	/* If no header was specified - assume that we want all entries */
	boolean_t csv_all = !csv->csv_ext_header || (csv->csv_ext_header && csv->csv_all);

	chars.csv_separator = csv->csv_separator;
	chars.csv_opt_separator = csv->csv_opt_separator;

	/* Parse header */
	bindings = mp_malloc(schema->hdr.count * sizeof(csv_binding_t));

	ret = csv_generate_bindings(&chars,
								(csv->csv_ext_header)? csv->csv_header : NULL, bindings, schema,
						  	    (csv_all)? CSV_HDR_GENERATE_ALL : CSV_HDR_GENERATE_OPT);

	if(ret < 1) {
		logmsg(LOG_CRIT, "CSV error %d", ret);
		goto end;
	}
	bcount = ret;

	if(!csv->csv_no_header)
		csv_write_header(&chars, backend->file, bindings, bcount);

	entries = mp_malloc(csv_entries_cache_count * entry_size);

	while(entry_idx < end) {
		entry_count = min((unsigned) (end - entry_idx), 
						  csv_entries_cache_count);

		ret = tsfile_get_entries(backend->ts_file, entries, entry_idx, entry_idx + entry_count);
		if(ret != TSFILE_OK) {
			logmsg(LOG_CRIT, "TSFile error %d", ret);
			goto end;
		}

		for(i = 0; i < entry_count; ++i) {
			entry = ((char*) entries) + i * entry_size;
			csv_write_entry(&chars, backend->file, bindings, bcount, entry);
		}

		entry_idx += entry_count;
	}

end:
	if(entries)
		mp_free(entries);

	mp_free(bindings);
	if(ret < 0)		/* Treat CSV errors (which are negative) as general error */
		return 1;
	return ret;
}

int tsf_csv_add(tsf_backend_t* backend) {
	struct tsf_csv_backend* csv = (struct tsf_csv_backend*) backend->private;

	csv_binding_t* bindings = NULL; 			/* bindings is a mapping between index in
												   CSV file and index of field in schema */
	int bcount;
	list_head_t list;
	tsfile_schema_t* schema = &backend->ts_file->header->schema;
	void* entry;

	int ret;

	char* line = NULL;
	char* read_ptr = NULL;
	size_t line_length, read_len;
	int lineno = (csv->csv_no_header)? 1 : 2;

	csv_chars_t chars;

	chars.csv_separator = csv->csv_separator;
	chars.csv_opt_separator = csv->csv_opt_separator;

	/* Parse header */
	bindings = mp_malloc(schema->hdr.count * sizeof(csv_binding_t));
	list_head_init(&list, "csv_list");

	ret = csv_read_header(&chars,
						  (csv->csv_no_header)? NULL : backend->file,
						  (csv->csv_ext_header)? csv->csv_header : NULL,
						  bindings, schema);

	if(ret < 1) {
		logmsg(LOG_CRIT, "CSV error %d at header", ret);
		goto end;
	}
	bcount = ret;

	line_length = csv_max_line_length(bindings, bcount);
	line = mp_malloc(line_length);

	while(feof(backend->file) == 0) {
		read_ptr = fgets(line, line_length, backend->file);
		if(read_ptr == NULL)
			break;

		/* Cut out LF */
		read_len = strlen(line);
		if(line[read_len - 1] == '\n')
			line[--read_len] = '\0';

		entry = csv_entry_create(&list, schema->hdr.entry_size);
		ret = csv_read_entry(&chars, line, bindings, bcount, entry);

		if(ret != CSV_OK) {
			logmsg(LOG_CRIT, "CSV error %d at line %d", ret, lineno);
			goto end;
		}
	}

	ret = csv_entry_list_add(backend->ts_file, &list);

end:
	if(line)
		mp_free(line);

	csv_entry_list_destroy(&list);

	mp_free(bindings);
	if(ret < 0)		/* Treat CSV errors (which are negative) as general error */
		return 1;
	return ret;
}

tsf_backend_class_t tsf_csv_backend_class = {
	tsf_csv_set, tsf_csv_get, tsf_csv_add
};

tsf_backend_t* tsfile_backend_create(const char* name) {
	tsf_backend_t* backend = mp_malloc(sizeof(tsf_backend_t));

	if(strncmp(name, "json", 4) == 0) {
		struct tsf_json_backend* json = mp_malloc(sizeof(struct tsf_json_backend));

		json->json_print_one = B_FALSE;

		if(strcmp(name, "jsonraw") == 0) {
			backend->tsf_class = &tsf_jsonraw_backend_class;
		}
		else {
			backend->tsf_class = &tsf_json_backend_class;
		}
		backend->private = json;
	}
	else if(strcmp(name, "csv") == 0) {
		struct tsf_csv_backend* csv = mp_malloc(sizeof(struct tsf_csv_backend));

		csv->csv_no_header = B_FALSE;
		csv->csv_ext_header = B_FALSE;
		csv->csv_all = B_FALSE;
		csv->csv_header[0] = '\0';

		csv->csv_separator = ',';
		csv->csv_opt_separator = ':';

		backend->tsf_class = &tsf_csv_backend_class;
		backend->private = csv;
	}
	else {
		mp_free(backend);
		return NULL;
	}

	backend->file = NULL;
	backend->ts_file = NULL;

	return backend;
}

int tsfile_backend_get(tsf_backend_t* backend, int start, int end) {
	if(start < 0 || end < 0) {
		logmsg(LOG_CRIT, "Entry indexes in TSFile range cannot be negative!");
		return 1;
	}
	
	if(end < start) {
		logmsg(LOG_CRIT, "Last entry index in TSFile range shouldn't precede first entry!");
		return 1;
	}
	
	return backend->tsf_class->get(backend, start, end);
}


void tsfile_backend_set_files(tsf_backend_t* backend, FILE* file, tsfile_t* ts_file) {
	backend->file = file;
	backend->ts_file = ts_file;
}

void tsfile_backend_destroy(tsf_backend_t* backend) {
	if(backend->private != NULL) {
		mp_free(backend->private);
	}

	mp_free(backend);
}

