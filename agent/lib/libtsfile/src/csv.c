
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



#define LOG_SOURCE "csv"
#include <log.h>

#include <defs.h>
#include <tsfile.h>
#include <mempool.h>
#include <list.h>
#include <field.h>

#include <csv.h>

#include <string.h>
#include <stdio.h>

const unsigned csv_entries_per_chunk = 8;

extern const char* tsfile_error_str[];

DECLARE_FIELD_FUNCTION_BYTE(uint8_t);
DECLARE_FIELD_FUNCTIONS(boolean_t);
DECLARE_FIELD_FUNCTIONS(uint16_t);
DECLARE_FIELD_FUNCTIONS(uint32_t);
DECLARE_FIELD_FUNCTIONS(uint64_t);
DECLARE_FIELD_FUNCTIONS(float);
DECLARE_FIELD_FUNCTIONS(double);

void* csv_entry_create(list_head_t* list, size_t entry_size) {
	csv_entry_chunk_t* entry_chunk;

	if(!list_empty(list)) {
		entry_chunk = list_last_entry(csv_entry_chunk_t, list, node);

		if(entry_chunk->count < csv_entries_per_chunk)
			return &entry_chunk->byte +
				   (entry_chunk->count++ * entry_size);
	}

	entry_chunk = mp_malloc(sizeof(csv_entry_chunk_t) +
							entry_size * csv_entries_per_chunk);

	list_node_init(&entry_chunk->node);
	entry_chunk->count = 1;

	list_add_tail(&entry_chunk->node, list);

	return &entry_chunk->byte;
}

int csv_entry_list_add(tsfile_t* ts_file, list_head_t* list) {
	csv_entry_chunk_t* entry_chunk;
	int err;

	list_for_each_entry(csv_entry_chunk_t, entry_chunk, list, node) {
		err = tsfile_add(ts_file, &entry_chunk->byte, entry_chunk->count);

		if(err != TSFILE_OK) {
			logmsg(LOG_CRIT, "Failed to add CSV entries. Error: %s",
				   tsfile_error_str[-tsfile_errno]);
			return CSV_INTERNAL_ERROR;
		}
	}

	return CSV_OK;
}

int csv_entry_list_destroy(list_head_t* list) {
	csv_entry_chunk_t *cur, *next;

	list_for_each_entry_safe(csv_entry_chunk_t, cur, next, list, node) {
		mp_free(cur);
	}

	return CSV_OK;
}

/* CSV header parser */


/**
 * CSV header iterator. Header is not modified, so instead of keeping
 * NULL-terminating strings, it keeps pointer to position in header line and
 * length of token.
 *
 * 	* _header_ - pointer to header line
 * 	* _tmp_ - pointer to next field
 *  * \[ptr ; ptr + name_length\) pointer to field name
 *  * \[opt ; opt + opt_length\) pointer to field option
 * */
typedef struct {
	const char* header;

	const char* ptr;
	const char* opt;
	const char* tmp;

	size_t length;
	size_t name_length;
	size_t opt_length;
} csv_hdr_iter_t;

static void csv_header_iter_init(csv_hdr_iter_t* iter, const char* header) {
	iter->header = header;

	iter->opt = iter->tmp = iter->ptr = header;
	iter->opt_length = iter->length = iter->name_length = 0;
}

static void csv_header_iter_next_opt(csv_chars_t* chars, csv_hdr_iter_t* iter) {
	char* opt2;

	if(!iter->opt)
		return;

	iter->opt = strchr(iter->opt, chars->csv_opt_separator);
	if(iter->opt != NULL &&
			(iter->tmp == NULL || iter->opt < iter->tmp)) {
		++iter->opt;

		opt2 = strchr(iter->opt, chars->csv_opt_separator);
		iter->opt_length =
			(opt2 != NULL && opt2 < iter->tmp)
				? opt2 - iter->opt
				: iter->length - (iter->opt - iter->ptr);
	}
	else {
		iter->opt = NULL;
		iter->opt_length = 0;
	}
}

static void csv_header_iter_next(csv_chars_t* chars, csv_hdr_iter_t* iter) {
	iter->opt = iter->ptr = iter->tmp;

	iter->tmp = strchr(iter->ptr, chars->csv_separator);
	iter->length =
		(iter->tmp != NULL)
			? iter->tmp - iter->ptr
			: strlen(iter->ptr);

	csv_header_iter_next_opt(chars, iter);

	iter->name_length =
		(iter->opt == NULL)
			? iter->length
			: iter->opt - iter->ptr - 1;

	if(iter->tmp != NULL)
		++iter->tmp;
}

static int csv_header_field_error(int error, csv_hdr_iter_t* iter, const char* format) {
	char field_name[MAXFIELDLEN];
	strncpy(field_name, iter->ptr, min(MAXFIELDLEN, iter->name_length));

	logmsg(LOG_CRIT, format, field_name, iter->ptr - iter->header);
	return error;
}

static boolean_t csv_header_field_eq(csv_hdr_iter_t* iter, tsfile_field_t* field) {
	return strncmp(field->name, iter->ptr, iter->name_length) == 0 &&
		   strlen(field->name) == iter->name_length;
}

#define CSV_PARSE_BOOL_OPT(iter, binding, literal, defval)			\
	do {															\
		if(iter->opt)  												\
			strncpy(binding->opt.bool.literal, iter->opt,			\
						min(iter->opt_length, CSVBOOLLEN));			\
		else														\
			strcpy(binding->opt.bool.literal, defval);				\
	} while(0)

#define CSV_PARSE_INT_OPT(iter, binding, name, flag)				\
		if(strncmp(iter->opt, name, iter->opt_length) == 0) {		\
			binding->opt.int_flags |= flag;							\
		}

static int csv_parse_header_opts(csv_chars_t* chars, csv_hdr_iter_t* iter, csv_binding_t* binding) {
	tsfile_field_t* field = binding->field;

	if(field->type == TSFILE_FIELD_BOOLEAN) {
		CSV_PARSE_BOOL_OPT(iter, binding, true_literal, "true");
		csv_header_iter_next_opt(chars, iter);
		CSV_PARSE_BOOL_OPT(iter, binding, false_literal, "false");
	}
	else if(field->type == TSFILE_FIELD_INT) {
		while(iter->opt != NULL) {
			CSV_PARSE_INT_OPT(iter, binding, "hex", CSV_INT_HEX);
			CSV_PARSE_INT_OPT(iter, binding, "unsigned", CSV_INT_UNSIGNED);

			csv_header_iter_next_opt(chars, iter);
		}
	}

	return CSV_OK;
}

static int csv_parse_header_field(csv_chars_t* chars, csv_hdr_iter_t* iter, csv_binding_t* bindings,
								  tsfile_schema_t* schema, csv_hdr_mode_t mode, int bcount) {
	int bid, fid, count;

	tsfile_field_t* field;

	count = schema->hdr.count;

	for(bid = 0; bid < bcount; ++bid) {
		if(csv_header_field_eq(iter, bindings[bid].field)) {
			if(mode == CSV_HDR_UPDATE) {
				return csv_parse_header_opts(chars, iter, bindings + bid);
			}
			else {
				return csv_header_field_error(CSV_HEADER_DUPLICATE, iter,
						"Header: field '%s' duplicate at %d");
			}
		}
	}

	if(mode == CSV_HDR_UPDATE)
		return csv_header_field_error(CSV_HEADER_MISSING, iter,
					"Header: field '%s' is missing at %d");


	for(fid = 0; fid < count; ++fid) {
		field = &schema->fields[fid];

		if(csv_header_field_eq(iter, field)) {
			bindings[bcount].field = field;

			return csv_parse_header_opts(chars, iter, bindings + bid);
		}
	}

	return csv_header_field_error(CSV_HEADER_INVALID, iter,
						"Header: unknown field '%s' at %d");
}

int csv_parse_header(csv_chars_t* chars, const char* header, csv_binding_t* bindings, tsfile_schema_t* schema,
					 csv_hdr_mode_t mode, int bcount) {
	int bid, fid, count;
	int binding_count;
	size_t length, name_length;

	csv_hdr_iter_t iter;

	int ret = CSV_OK;

	csv_header_iter_init(&iter, header);

	if(mode == CSV_HDR_UPDATE) {
		count = bcount;
	}
	else if(mode == CSV_HDR_PARSE_ALL || mode == CSV_HDR_PARSE_OPT) {
		count = schema->hdr.count;
		memset(bindings, 0, sizeof(csv_binding_t) * count);
	}
	else {
		return CSV_INTERNAL_ERROR;
	}

	for(bid = 0; bid < count; ++bid) {
		if(iter.tmp == NULL) {
			if(mode == CSV_HDR_PARSE_ALL) {
				logmsg(LOG_CRIT, "Header: some fields are missing");
				return CSV_HEADER_MISSING;
			}
			else {
				return bcount;
			}
		}

		csv_header_iter_next(chars, &iter);

		ret = csv_parse_header_field(chars, &iter, bindings, schema, mode, bcount);

		if(mode != CSV_HDR_UPDATE)
			++bcount;

		if(ret != CSV_OK)
			return ret;
	}

	return bcount;
}

/**
 * Read header from file or from command line option
 *
 * If file doesn't contain header (and `file == NULL)`, header is specified by
 * option header. If `file != NULL`, csv_read_header reads it from file first,
 * then updates it with data from **header**
 *
 * @param chars csv characters
 * @param file CSV file
 * @param header Header line (from command line option)
 * @param bindings Destination array of bindings
 * @param schema TSF Schema
 *
 * @return count of bindings that set or negative error value.
 */
int csv_read_header(csv_chars_t* chars, FILE* file, const char* header, csv_binding_t* bindings, tsfile_schema_t* schema) {
	char file_header[CSV_HEADER_LENGTH];
	int ret = CSV_OK;
	size_t read_len = 0;

	if(file != NULL) {
		if(fgets(file_header, CSV_HEADER_LENGTH, file) == NULL) {
			logmsg(LOG_CRIT, "Header: read error");
			return CSV_READ_ERROR;
		}

		read_len = strlen(file_header);
		if(file_header[read_len - 1] == '\n')
			file_header[--read_len] = '\0';

		ret = csv_parse_header(chars, file_header, bindings, schema, CSV_HDR_PARSE_ALL, 0);

		if(ret < CSV_OK)
			return ret;

		if(header != NULL)
			return csv_parse_header(chars, header, bindings, schema, CSV_HDR_UPDATE, ret);

		return ret;
	}

	if(header == NULL)
		return CSV_INTERNAL_ERROR;

	return csv_parse_header(chars, header, bindings, schema, CSV_HDR_PARSE_ALL, 0);
}

/**
 * Generate bindings from schema and optional header
 *
 * @param header Header line (from command line option)
 * @param bindings Destination array of bindings
 * @param schema TSF Schema
 */
int csv_generate_bindings(csv_chars_t* chars, const char* header, csv_binding_t* bindings,
		tsfile_schema_t* schema, csv_hdr_mode_t mode) {
	int count = schema->hdr.count;
	int bid, bcount = 0;
	int ret;

	if(mode == CSV_HDR_GENERATE_ALL) {
		memset(bindings, 0, sizeof(csv_binding_t) * count);

		for(bid = 0; bid < count; ++bid) {
			bindings[bid].field = &schema->fields[bid];
		}

		if(header != NULL) {
			ret = csv_parse_header(chars, header, bindings, schema, CSV_HDR_UPDATE, count);

			if(ret != CSV_OK)
				return ret;
		}

		return count;
	}

	return csv_parse_header(chars, header, bindings, schema, CSV_HDR_PARSE_OPT, 0);
}

/**
 * Write header to file
 */
void csv_write_header(csv_chars_t* chars, FILE* file, csv_binding_t* bindings, int bcount) {
	int bid;

	tsfile_field_t* field;

	for(bid = 0; bid < bcount; ++bid) {
		field = bindings[bid].field;

		fputs(field->name, file);

		if(field->type == TSFILE_FIELD_BOOLEAN) {
			fprintf(file, "%c%s%c%s", chars->csv_opt_separator, bindings[bid].opt.bool.true_literal,
					chars->csv_opt_separator, bindings[bid].opt.bool.false_literal);
		}
		else if(field->type == TSFILE_FIELD_INT) {
			if(bindings[bid].opt.int_flags & CSV_INT_HEX) {
				fprintf(file, "%chex", chars->csv_opt_separator);
			}
			if(bindings[bid].opt.int_flags & CSV_INT_UNSIGNED) {
				fprintf(file, "%cunsigned", chars->csv_opt_separator);
			}
		}

		if(bid < (bcount - 1))
			fputc(chars->csv_separator, file);
	}

	fputc('\n', file);
}

size_t csv_max_line_length(csv_binding_t* bindings, int bcount) {
	size_t max_length = bcount + 1;
	int bid;

	tsfile_field_t* field;

	for(bid = 0; bid < bcount; ++bid) {
		field = bindings[bid].field;

		switch(field->type) {
			case TSFILE_FIELD_BOOLEAN:
				max_length += CSVBOOLLEN;
			break;
			case TSFILE_FIELD_INT:
			{
				switch(field->size) {
				case 1:
					max_length += 4;
				break;
				case 2:
					max_length += 6;
				break;
				case 4:
					max_length += 11;
				break;
				case 8:
					max_length += 21;
				break;
				}
			}
			break;
			case TSFILE_FIELD_FLOAT:
			{
				switch(field->size) {
				case sizeof(float):
					max_length += 64;
				break;
				case sizeof(double):
					/* ~strlen(-DBL_MAX) */
					max_length += 320;
				break;
				}
			}
			break;
			case TSFILE_FIELD_STRING:
				/* Worst case when string constists only from DQUOTEs */
				max_length += 2 * (field->size + 1);
			break;
		}
	}

	return max_length;
}

static const char* csv_int_format_table[4][6] = {
	/* size	{ decimal, hex, unsigned } */
	/* 1 */ { "%" PRId8,  "%" PRIx8,  "%" PRIu8,
			  "%" SCNd8,  "%" SCNx8,  "%" SCNu8 },
	/* 2 */ { "%" PRId16, "%" PRIx16, "%" PRIu16,
			  "%" SCNd16, "%" SCNx16, "%" SCNu16 },
	/* 8 */ { "%" PRId64, "%" PRIx64, "%" PRIu64,
			  "%" SCNd64, "%" SCNx64, "%" SCNu64 },
	/* 4 */ { "%" PRId32, "%" PRIx32, "%" PRIu32,
			  "%" SCNd32, "%" SCNx32, "%" SCNu32 }
};

static const char* csv_int_format_str(csv_binding_t* binding, boolean_t is_scanf) {
	int i, j;
	int int_flags = binding->opt.int_flags;

	i = (binding->field->size == 8)
			? 2
			: binding->field->size - 1;

	j = (int_flags & CSV_INT_HEX)
			? 1
			: (int_flags & CSV_INT_UNSIGNED)
			  ? 2
			  :	0;

	j = (is_scanf)? j + 3 : j;

	return csv_int_format_table[i][j];
}

static void csv_write_string(csv_chars_t* chars, FILE* file, const char* string) {
	const char* ptr = strchr(string, chars->csv_separator);

	if(ptr == NULL) {
		fputs(string, file);
		return;
	}

	ptr = string;

	/* If commas are in string - escape double quotas */
	fputc('"', file);
	while(*ptr) {
		fputc(*ptr, file);
		if(*ptr == '"')
			fputc('"', file);

		++ptr;
	}
	fputc('"', file);
}

static size_t csv_read_string(csv_chars_t* chars, const char* ptr, char* string, size_t str_length) {
	const char* src = ptr;
	char* dst = string;
	size_t length = str_length;

	boolean_t unescape = B_FALSE;

	if(*src == '"') {
		/* Unescape */
		while(*src) {
			if(*src == '"') {
				++src;

				/* Reached end-of field */
				if(*src == chars->csv_separator || !*src)
					break;
			}

			if(length > 0) {
				*dst = *src;
				++dst;
				--length;
			}

			++src;
		}
	}
	else {
		while(*src != chars->csv_separator && *src) {
			if(length > 0) {
				*dst = *src;
				++dst;
				--length;
			}

			++src;
		}
	}

	*dst = '\0';

	return src - ptr + 1;
}

void csv_write_entry(csv_chars_t* chars, FILE* file, csv_binding_t* bindings, int bcount, void* entry) {
	int bid;

	tsfile_field_t* field;
	char* value;

	for(bid = 0; bid < bcount; ++bid) {
		field = bindings[bid].field;
		value = ((char*) entry) + field->offset;

		switch(field->type) {
		case TSFILE_FIELD_BOOLEAN:
				fputs((FIELD_GET_VALUE(boolean_t, value))
					   ? bindings[bid].opt.bool.true_literal
					   : bindings[bid].opt.bool.false_literal, file);
			break;
		case TSFILE_FIELD_INT:
			{
			const char* fmt = csv_int_format_str(&bindings[bid], B_FALSE);
			switch(field->size) {
				case 1:
					fprintf(file, fmt, FIELD_GET_VALUE(uint8_t, value));
				break;
				case 2:
					fprintf(file, fmt, FIELD_GET_VALUE(uint16_t, value));
				break;
				case 4:
					fprintf(file, fmt, FIELD_GET_VALUE(uint32_t, value));
				break;
				case 8:
					fprintf(file, fmt, FIELD_GET_VALUE(uint64_t, value));
				break;
			}
			}
			break;
		case TSFILE_FIELD_FLOAT:
			switch(field->size) {
			case sizeof(float):
				fprintf(file, "%f", FIELD_GET_VALUE(float, value));
			break;
			case sizeof(double):
				fprintf(file, "%f", FIELD_GET_VALUE(double, value));
			break;
			}
			break;
		case TSFILE_FIELD_STRING:
			csv_write_string(chars, file, value);
		break;
		}

		if(bid < (bcount - 1))
			fputc(chars->csv_separator, file);
	}

	fputc('\n', file);
}

int csv_read_entry(csv_chars_t* chars, const char* line, csv_binding_t* bindings, int bcount, void* entry) {
	int bid;

	tsfile_field_t* field;
	char* value;

	size_t pos = 0;
	int scan_len;
	int scan_count = 0;
	size_t line_len = strlen(line);

	char bool_value[CSVBOOLLEN];
	char fmttmp[12];
	const char* fmtstr;

	for(bid = 0; bid < bcount; ++bid) {
		field = bindings[bid].field;
		value = ((char*) entry) + field->offset;

		if(pos == line_len) {
			logmsg(LOG_CRIT, "Line ended prematurely at %lld", pos);
			return CSV_PARSE_LINE_ERROR;
		}
		if(pos > line_len) {
			logmsg(LOG_CRIT, "Internal error: 'pos' %lld is outside line", pos);
			return CSV_INTERNAL_ERROR;
		}

		if(field->type == TSFILE_FIELD_STRING) {
			pos += csv_read_string(chars, line + pos, value, field->size);
			continue;
		}
		else if(field->type == TSFILE_FIELD_BOOLEAN) {
			pos += csv_read_string(chars, line + pos, bool_value, field->size);

			if(strncmp(bool_value, bindings[bid].opt.bool.true_literal, CSVBOOLLEN) == 0) {
				FIELD_PUT_VALUE(boolean_t, value, B_TRUE);
				continue;
			}
			else if (strncmp(bool_value, bindings[bid].opt.bool.false_literal, CSVBOOLLEN) == 0) {
				FIELD_PUT_VALUE(boolean_t, value, B_FALSE);
				continue;
			}
			else {
				logmsg(LOG_CRIT, "Boolean parse error, unexpected literal '%s' at %d",
						bool_value, (int) pos);
				return CSV_PARSE_BOOL_ERROR;
			}
		}

		if(field->type == TSFILE_FIELD_INT) {
			fmtstr = csv_int_format_str(&bindings[bid], B_TRUE);
		}
		else if(field->type == TSFILE_FIELD_FLOAT) {
			fmtstr = "%f";
		}

		if(bid < (bcount - 1)) {
			sprintf(fmttmp, "%s%c%%n", fmtstr, chars->csv_separator);
			fmtstr = fmttmp;
		}
		else {
			sprintf(fmttmp, "%s%%n", fmtstr);
			fmtstr = fmttmp;
		}

		scan_count = sscanf(line + pos, fmtstr, value, &scan_len);

		if(scan_count == 0) {
			logmsg(LOG_CRIT, "Failed to parse field at %lld. Format: %s", pos, fmtstr);
			return CSV_PARSE_FMT_ERROR;
		}

		pos += scan_len;
	}

	return CSV_OK;
}


