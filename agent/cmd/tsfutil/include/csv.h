/*
 * csv.h
 *
 *  Created on: Jan 16, 2014
 *      Author: myaut
 */

#ifndef CSV_H_
#define CSV_H_

#include <tsfile.h>

#define CSV_HEADER_LENGTH	1024
#define CSVBOOLLEN			12

#define CSV_OK				 0

#define CSV_INTERNAL_ERROR   -1

#define CSV_HEADER_DUPLICATE -2
#define CSV_HEADER_INVALID	 -3
#define CSV_HEADER_MISSING	 -4

#define CSV_PARSE_BOOL_ERROR -5
#define CSV_PARSE_LINE_ERROR -6
#define CSV_PARSE_FMT_ERROR  -7

#define CSV_READ_ERROR		 -8
#define CSV_WRITE_ERROR		 -9

typedef struct {
	list_node_t		node;
	int				count;

	char			byte[1];
} csv_entry_chunk_t;

#define	CSV_INT_HEX				0x01
#define	CSV_INT_UNSIGNED		0x02

typedef struct {
	tsfile_field_t*	field;

	union {
		struct {
			char true_literal[CSVBOOLLEN];
			char false_literal[CSVBOOLLEN];
		} bool;
		int int_flags;
	} opt;
} csv_binding_t;

typedef enum {
	CSV_HDR_PARSE_ALL,
	CSV_HDR_PARSE_OPT,

	CSV_HDR_GENERATE_ALL,
	CSV_HDR_GENERATE_OPT,

	CSV_HDR_UPDATE
} csv_hdr_mode_t;

void* csv_entry_create(list_head_t* list, size_t entry_size);

int csv_entry_list_add(tsfile_t* ts_file, list_head_t* list);
int csv_entry_list_destroy(list_head_t* list);

size_t csv_max_line_length(csv_binding_t* bindings, int bcount);
int csv_read_header(FILE* file, const char* header, csv_binding_t* bindings,
					tsfile_schema_t* schema);
int csv_read_entry(const char* line, csv_binding_t* bindings, int bcount, void* entry);

int csv_generate_bindings(const char* header, csv_binding_t* bindings,
						  tsfile_schema_t* schema, csv_hdr_mode_t mode);
void csv_write_header(FILE* file, csv_binding_t* bindings, int bcount);
void csv_write_entry(FILE* file, csv_binding_t* bindings, int bcount, void* entry);

#endif /* CSV_H_ */
