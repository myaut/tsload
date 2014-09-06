/*
 * wlparam.h
 *
 *  Created on: 08.11.2012
 *      Author: myaut
 */

#ifndef WLPARAM_H_
#define WLPARAM_H_

#include <defs.h>
#include <list.h>
#include <randgen.h>

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
 * Workload parameter type definitions
 */
typedef int64_t		wlp_integer_t;
typedef double		wlp_float_t;
typedef char		wlp_string_t;
typedef boolean_t	wlp_bool_t;
typedef int  		wlp_strset_t;
typedef void*		wlp_hiobject_t;

/**
 * Workload paramter type
 * [wlparam-type]
 *
 * @value WLP_NULL	Used to mark end of param list
 * @value WLP_BOOL  Boolean type
 * @value WLP_INTEGER Integer type
 * @value WLP_FLOAT Floating point type
 * @value WLP_RAW_STRING String. Use range to define maximum length
 * @value WLP_STRING_SET Depending on input strings, selects one of possible outcomes.
 */
typedef enum {
	WLP_NULL,

	WLP_BOOL,
	WLP_INTEGER,
	WLP_FLOAT,
	WLP_RAW_STRING, /*Any string*/

	WLP_STRING_SET,  /*string - one of possible values*/

	/* metatypes - using primitive types in serialization
	 * but have different meanings */
	WLP_SIZE,		/* size - in Kb, etc. */
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
			char** ss_strings;
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
 * Range declaration
 * [wlparam-range]
 */
#define WLP_NO_RANGE()				{ B_FALSE, { {0} } }
#define WLP_STRING_LENGTH(length) 	{ B_TRUE, { {length}, {0, 0}, {0.0, 0.0}, {0, NULL} } }
#define WLP_INT_RANGE(min, max)  	{ B_TRUE, { {0}, {min, max} } }
#define WLP_FLOAT_RANGE(min, max) 	{ B_TRUE, { {0}, {0, 0}, {min, max} } }
#define WLP_STRING_SET_RANGE(set) 	{ B_TRUE, { {0}, {0, 0}, {0.0, 0.0},  		\
									    {sizeof((set)) / sizeof(char*), (set) } } }

/**
 * Default wl parameter value
 * [wlparam-default]
 */
#define WLP_NO_DEFAULT()				{ B_FALSE, B_FALSE }
#define WLP_BOOLEAN_DEFAULT(b)			{ B_TRUE, b }
#define WLP_INT_DEFAULT(i)				{ B_TRUE, B_FALSE, i }
#define WLP_FLOAT_DEFAULT(f)			{ B_TRUE, B_FALSE, 0, f }
#define WLP_STRING_DEFAULT(s)			{ B_TRUE, B_FALSE, 0, 0.0, s }
#define WLP_STRING_SET_DEFAULT(ssi)		{ B_TRUE, B_FALSE, 0, 0.0, NULL, ssi }

/**
 * Workload parameter flags
 * [wlparam-flags]
 */
#define WLPF_NO_FLAGS				0x00
#define WLPF_OPTIONAL				0x01
#define WLPF_REQUEST				0x02
#define WLPF_OUTPUT					(WLPF_REQUEST | 0x04)

/**
 * Workload parameter descriptor
 *
 * @member type Parameter type
 * @member flags
 * @member off Offset in data structure where param value would be written */
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

#define WLP_ERROR_PREFIX 			"Failed to set workload '%s' parameter '%s': "
#define WLP_PMAP_ERROR_PREFIX		WLP_ERROR_PREFIX "pmap element #%d: "

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

#ifndef NO_JSON
#include <libjson.h>

#define WLPARAM_JSON_OK		0

STATIC_INLINE int json_wlparam_string_proc(JSONNODE* node, wlp_descr_t* wlp, void* param) { return -1; }
STATIC_INLINE int json_wlparam_proc(JSONNODE* node, wlp_descr_t* wlp, void* param) { return -1; }

STATIC_INLINE int json_wlpgen_proc(JSONNODE* node, wlp_descr_t* wlp, struct workload* wl) {return -1; }

STATIC_INLINE JSONNODE* json_wlparam_format(wlp_descr_t* wlp) { return NULL; }
STATIC_INLINE JSONNODE* json_wlparam_format_all(wlp_descr_t* wlp) { return NULL; }
STATIC_INLINE int json_wlparam_proc_all(JSONNODE* node, wlp_descr_t* wlp, struct workload* wl) { return NULL; }
#else
#include <tsobj.h>
TESTEXPORT int tsobj_wlparam_proc(tsobj_node_t* node, wlp_descr_t* wlp, void* param, struct workload* wl);
TESTEXPORT int tsobj_wlpgen_proc(tsobj_node_t* node, wlp_descr_t* wlp, struct workload* wl);

TESTEXPORT int tsobj_wlparam_proc_all(tsobj_node_t* node, wlp_descr_t* wlp, struct workload* wl);
#endif

#endif /* WLPARAM_H_ */
