
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

#include <tsload/tuneit.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>


static boolean_t tuneit_initialized = B_FALSE;
static list_head_t tuneit_opt_list;

static tuneit_opt_t* tuneit_find_opt(const char* name);
static void tuneit_opt_destroy(tuneit_opt_t* opt);
static void tuneit_error(const char* name, const char* format, ...);

int tuneit_add_option(char* optstr) {
	tuneit_opt_t* opt = malloc(sizeof(tuneit_opt_t));
	char* eq;

	if(!tuneit_initialized) {
		list_head_init(&tuneit_opt_list, "tuneit_opt_list");
		tuneit_initialized = B_TRUE;
	}

	opt->buf = malloc(strlen(optstr) + 1);
	strcpy(opt->buf, optstr);

	opt->name = opt->buf;
	opt->value = NULL;

	if(*opt->buf == '+') {
		++opt->name;
		opt->bval = B_TRUE;
	}
	else if(*opt->buf == '-') {
		++opt->name;
		opt->bval = B_FALSE;
	}
	else {
		eq = strchr(opt->buf, '=');
		opt->bval = B_TRUE;

		if(eq != NULL) {
			*eq = '\0';
			opt->value = eq + 1;
		}
	}

	list_node_init(&opt->node);
	list_add_tail(&opt->node, &tuneit_opt_list);

	return TUNEIT_OK;
}
static const char* tuneit_int_format_table[4][3] = {
	/* size	{ decimal, hex, octal } */
	/* 1 */ { "%" SCNd8,  "%" SCNx8,  "%" SCNo8 },
	/* 2 */ { "%" SCNd16, "%" SCNx16, "%" SCNo16 },
	/* 8 */ { "%" SCNd64, "%" SCNx64, "%" SCNo64 },
	/* 4 */ { "%" SCNd32, "%" SCNx32, "%" SCNo32 }
};

int tuneit_set_int_impl(const char* name, size_t sz, void* ptr) {
	int ret = TUNEIT_OK;
	tuneit_opt_t* opt = tuneit_find_opt(name);

	int row, col;
	int count;
	char* value;

#ifdef _MSC_VER
	unsigned temp;
	void* ptr1;
#endif

	if(opt == NULL)
		return TUNEIT_NOTSET;

	if(opt->value == NULL) {
		tuneit_error(name, "missing value");
		ret = TUNEIT_INVALID;
		goto end;
	}

	row = (sz == 8)? 2 : (sz - 1);
	if(*opt->value == '0') {
		if(*(opt->value + 1) == 'x') {
			/* '0x' prefix --> hexademical values */
			col = 1;
			value = opt->value + 2;
		}
		else if(*(opt->value + 1) == 'o') {
			/* '0o' prefix --> octal values */
			col = 2;
			value = opt->value + 2;
		}
		else {
			/* '0' prefix --> octal values */
			col = 2;
			value = opt->value + 1;
		}
	}
	else {
		col = 0;
		value = opt->value;
	}

#ifdef _MSC_VER
	/* Visual C doesn't support 1-byte sscanf, so use temporary integer variable */
	if(sz == 1) {
		ptr1 = ptr;
		ptr = &temp;
	}
#endif

	count = sscanf(value, tuneit_int_format_table[row][col], ptr);

#ifdef _MSC_VER
	if(sz == 1) {
		*((unsigned char*) ptr1) = (unsigned char) temp;
	}
#endif

	if(count != 1) {
		tuneit_error(name, "failed to parse integer '%s'", value);
		ret = TUNEIT_INVALID;
	}

	/* FIXME: check ERANGE here */

end:
	tuneit_opt_destroy(opt);

	return ret;
}

int tuneit_set_bool_impl(const char* name, boolean_t* ptr) {
	int ret = TUNEIT_OK;
	tuneit_opt_t* opt = tuneit_find_opt(name);

	if(opt == NULL)
		return TUNEIT_NOTSET;

	if(opt->value != NULL) {
		if(strcmp(opt->value, "true") == 0) {
			*ptr = B_TRUE;
		}
		else if(strcmp(opt->value, "false") == 0) {
			*ptr = B_FALSE;
		}
		else {
			tuneit_error(name, "invalid boolean literal '%s'", opt->value);
			ret = TUNEIT_INVALID;
		}
	}
	else {
		*ptr = opt->bval;
	}

end:
	tuneit_opt_destroy(opt);

	return ret;
}

int tuneit_set_string_impl(const char* name, char* ptr, size_t length) {
	int ret = TUNEIT_OK;
	tuneit_opt_t* opt = tuneit_find_opt(name);

	if(opt == NULL)
		return TUNEIT_NOTSET;

	if(opt->value == NULL) {
		tuneit_error(name, "missing value");
		ret = TUNEIT_INVALID;
		goto end;
	}

	if(strlen(opt->value) > (length - 1)) {
		tuneit_error(name, "too long string, maximum length: %d", (int) (length - 1));
		ret = TUNEIT_INVALID;
		goto end;
	}

	strcpy(ptr, opt->value);

end:
	tuneit_opt_destroy(opt);

	return ret;
}

int tuneit_finalize(void) {
	int ret = TUNEIT_OK;
	tuneit_opt_t* opt;
	tuneit_opt_t* next;

	if(!tuneit_initialized) {
		return TUNEIT_OK;
	}

	list_for_each_entry_safe(tuneit_opt_t, opt, next, &tuneit_opt_list, node) {
		fprintf(stderr, "Unknown tuneable option '%s'\n", opt->name);

		ret = TUNEIT_UNKNOWN;
		tuneit_opt_destroy(opt);
	}

	return ret;
}

static tuneit_opt_t* tuneit_find_opt(const char* name) {
	tuneit_opt_t* opt;

	if(!tuneit_initialized) {
		return NULL;
	}

	list_for_each_entry(tuneit_opt_t, opt, &tuneit_opt_list, node) {
		if(strcmp(name, opt->name) == 0) {
			return opt;
		}
	}

	return NULL;
}

static void tuneit_opt_destroy(tuneit_opt_t* opt) {
	list_del(&opt->node);

	free(opt->buf);
	free(opt);
}

static void tuneit_error(const char* name, const char* format, ...) {
	va_list va;

	fprintf(stderr, "Couldn't set tuneable option '%s': ", name);

	va_start(va, format);
	vfprintf(stderr, format, va);
	va_end(va);

	fputc('\n', stderr);
}

