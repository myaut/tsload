
/*
    This file is part of TSLoad.
    Copyright 2012-2014, Sergey Klyaus, ITMO University

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



#ifndef WLPARAM_H_
#define WLPARAM_H_

#include <tsload/defs.h>

#include <tsload/list.h>

#include <tsload/obj/obj.h>

#include <tsload/load/randgen.h>

#include <stddef.h>
#include <stdint.h>


/**
 * @module Workload parameters
 *
 * These declarations used to provide module description to server
 * and simply parse workload parameters in JSON format.
 *
 * Each parameter declaration has:
 * 	- type @see wlp_type_t
 * 	- limitations (like string max length, number range, possible values, etc.) @see wlp_range_t and macroses
 * 	- name and description (text)
 * 	- offset in control structure which is used during parsing
 *
 * Parameter declaration are saved as array of wlp_descr_t structures with "NULL-terminator" with type WLP_NULL.
 * */

struct workload;

/**
 * [wlparam-typedef]
 *
 * Workload parameter type definitions
 */
typedef int64_t		wlp_integer_t;
typedef double		wlp_float_t;
typedef char		wlp_string_t;
typedef boolean_t	wlp_bool_t;
typedef int  		wlp_strset_t;
typedef void*		wlp_hiobject_t;

/**
 * [wlparam-type]
 *
 * Workload parameter type hint.
 *
 * When TSLoad generates a value that is passed to a module as a workload or a request
 * parameter, it relies on that hint to correctly handle incoming value and write it to
 * memory.
 *
 * There are "base" types which define that factors but doesn't know nature of a
 * value and a "meta" types that add meaning to a value. Meta-types rely on a corresponding
 * base value in how they handled but add some meaning to values.
 *
 * I.e. there is WLP_INTEGER that is used for keeping integers (base type) that expect
 * "integer" number from config and wlp_integer_t residing in data structure. But if it
 * represent a size (i.e. size of disk block) it is reasonable to use WLP_SIZE - metatype
 * that has same constraints as WLP_INTEGER but may provide nice formatting (i.e. "8 kb")
 * in some cases.
 *
 * @value WLP_NULL Special value - used to mark end of parameter list
 * @value WLP_TYPE_MAX Special value - maximum number of types
 * @value WLP_BOOL Boolean type (wlp_bool_t)
 * @value WLP_INTEGER Integer numbers (wlp_integer_t)
 * @value WLP_FLOAT Floating point number (wlp_float_t)
 * @value WLP_RAW_STRING String, statically allocated (char[]). Use range to provide its length
 * @value WLP_STRING_SET Enumeration (int/wlp_strset_t) - parses incoming string and picks index \
 * 						 of it in array defined by range.
 * @value WLP_SIZE Volume of digital information in bytes (wlp_integer_t)
 * @value WLP_TIME Time intervals (ts_time_t)
 * @value WLP_FILE_PATH Path on a file system (char[])
 * @value WLP_CPU_OBJECT HostInfo object representing a processor object (hi_object_t*)
 * @value WLP_DISK HostInfo object representing a disk, volume, etc. (hi_object_t*)
 */
typedef enum {
	WLP_NULL,

	WLP_BOOL,
	WLP_INTEGER,
	WLP_FLOAT,
	WLP_RAW_STRING, 	/*Any string*/

	WLP_STRING_SET,  	/*string - one of possible values*/

	/* metatypes - using primitive types in serialization
	 * but have different meanings */
	WLP_SIZE,
	WLP_TIME,

	WLP_FILE_PATH,
	WLP_CPU_OBJECT,
	WLP_DISK,

	WLP_TYPE_MAX,

	WLP_HI_OBJECT		/* Base type - not really used */
} wlp_type_t;

typedef struct {
	boolean_t range;		/*enabled flag*/

	/* Here should be union, but because of some smart people, who decided that
	 * | ISO C++03 8.5.1[dcl.init.aggr]/15:
	 * | When a union is initialized with a brace-enclosed initializer,
	 * | the braces shall only contain an initializer for the first member of the union.
	 * and another smart people from Microsoft who ignoring C99 compliance
	 * we will waste memory to provide nice macros like WLP_STRING_LENGTH
	 *  */
	struct {
		/*WLP_RAW_STRING*/
		struct {
			unsigned str_length;
		};
		/*WLP_INTEGER*/
		struct {
			wlp_integer_t i_min;
			wlp_integer_t i_max;
		};
		/*WLP_FLOAT*/
		struct {
			wlp_float_t d_min;
			wlp_float_t d_max;
		};
		/*WLP_STRING_SET*/
		struct {
			int ss_num;
			const char** ss_strings;
		};
	};
} wlp_range_t;

typedef struct {
	boolean_t enabled;

	wlp_bool_t b;
	wlp_integer_t i;
	wlp_float_t f;
	char* s;
	wlp_strset_t ssi;
} wlp_default_t;

/**
 * [wlparam-range]
 *
 * Range declaration
 */
#define WLP_NO_RANGE()				{ B_FALSE, { {0} } }
#define WLP_STRING_LENGTH(length) 	{ B_TRUE, { {length}, {0, 0}, {0.0, 0.0}, {0, NULL} } }
#define WLP_INT_RANGE(min, max)  	{ B_TRUE, { {0}, {min, max} } }
#define WLP_FLOAT_RANGE(min, max) 	{ B_TRUE, { {0}, {0, 0}, {min, max} } }
#define WLP_STRING_SET_RANGE(set) 	{ B_TRUE, { {0}, {0, 0}, {0.0, 0.0},  		\
									    {sizeof((set)) / sizeof(char*), (set) } } }

/**
 * [wlparam-default]
 *
 * Default wl parameter value
 */
#define WLP_NO_DEFAULT()				{ B_FALSE, B_FALSE }
#define WLP_BOOLEAN_DEFAULT(b)			{ B_TRUE, b }
#define WLP_INT_DEFAULT(i)				{ B_TRUE, B_FALSE, i }
#define WLP_FLOAT_DEFAULT(f)			{ B_TRUE, B_FALSE, 0, f }
#define WLP_STRING_DEFAULT(s)			{ B_TRUE, B_FALSE, 0, 0.0, s }
#define WLP_STRING_SET_DEFAULT(ssi)		{ B_TRUE, B_FALSE, 0, 0.0, NULL, ssi }

/**
 * [wlparam-flags]
 *
 * Workload parameter flags
 */
#define WLPF_NO_FLAGS				0x00
#define WLPF_OPTIONAL				0x01
#define WLPF_REQUEST				0x02
#define WLPF_OUTPUT					(WLPF_REQUEST | 0x04)

/**
 * Workload parameter descriptor
 *
 * @member type Parameter type hint
 * @member flags Parameter flags
 * @member range Pre-defined range of acceptable parameter values. If set, TSLoad will pre-check 				\
 * 				 value passed from config or a server and will raise an error, if it is outside this range.
 * @member defval Default value of parameter. Could be a hint, or if WLPF_OPTIONAL flag is set, could really	\
 * 				  be a default value that would be used if parameter is omitted from config.
 * @member name Name of parameter
 * @member description Description of a parameter
 * @member off Offset in data structure where param value would be written
 * */
typedef struct {
	wlp_type_t type;
	unsigned long flags;

	wlp_range_t range;
	wlp_default_t defval;

	char* name;
	char* description;

	size_t off;
} wlp_descr_t;

#define WLPARAM_DEFAULT_OK			0
#define WLPARAM_TSOBJ_OK			0
#define WLPARAM_TSOBJ_BAD			-1
#define WLPARAM_OUTSIDE_RANGE		-2
#define WLPARAM_NO_DEFAULT			-4
#define WLPARAM_ERROR				-5

typedef enum wlpgen_type {
	WLPG_VALUE,
	WLPG_RANDOM
} wlpgen_type_t;

typedef union wlpgen_value {
	char value[16];
	char* string;
} wlpgen_value_t;

typedef struct {
	wlpgen_value_t  value;

	wlpgen_value_t* valarray;
	int				length;

	double probability;
} wlpgen_probability_t;

typedef struct wlpgen_randgen {
	randgen_t*	rg;
	randvar_t* 	rv;

	/* For boolean/stringset - probability map */
	int pcount;
	wlpgen_probability_t* pmap;
} wlpgen_randgen_t;

/* Generator for per-request params */
typedef struct wlp_generator {
	wlpgen_type_t type;
	wlp_descr_t* wlp;
	struct workload* wl;

	union {
		wlpgen_value_t value;
		wlpgen_randgen_t randgen;
	} generator;

	list_node_t	node;
} wlp_generator_t;

LIBEXPORT wlp_type_t wlp_get_base_type(wlp_descr_t* wlp);

int wlparam_set_default(wlp_descr_t* wlp, void* param, struct workload* wl);

int wlpgen_create_default(wlp_descr_t* wlp, struct workload* wl);
TESTEXPORT void wlpgen_destroy_all(struct workload* wl);
void* wlpgen_generate(struct workload* wl);

tsobj_node_t* tsobj_wlparam_format_all(wlp_descr_t* wlp);

TESTEXPORT int tsobj_wlparam_proc(tsobj_node_t* node, wlp_descr_t* wlp, void* param, struct workload* wl);
TESTEXPORT int tsobj_wlpgen_proc(tsobj_node_t* node, wlp_descr_t* wlp, struct workload* wl);

TESTEXPORT int tsobj_wlparam_proc_all(tsobj_node_t* node, wlp_descr_t* wlp, struct workload* wl);

#endif /* WLPARAM_H_ */

