/*
 * wlparam.h
 *
 *  Created on: 08.11.2012
 *      Author: myaut
 */

#ifndef WLPARAM_H_
#define WLPARAM_H_

#include <stddef.h>
#include <stdint.h>
#include <defs.h>

/* Workload parameter declaration
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

typedef long	wlp_integer_t;
typedef double	wlp_float_t;
typedef char	wlp_string_t;
typedef uint64_t	wlp_size_t;
typedef int  	wlp_bool_t;
typedef int  	wlp_strset_t;

typedef enum {
	/*NULL-term*/	WLP_NULL,		/*Used to mark end of param list*/

	/*int*/			WLP_BOOL,
	/*long*/		WLP_INTEGER,
	/*double*/		WLP_FLOAT,
	/*char[]*/ 		WLP_RAW_STRING, /*Any string*/

	/*size_t*/		WLP_SIZE,		/*size_t. May be 4k, 4M or 4G*/
	/*char[]*/		WLP_STRING_SET  /*string - one of posible value*/
} wlp_type_t;

typedef struct {
	boolean_t range;		/*enabled flag*/

	/* Here should be union, but because of some smart people, who desided that
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
		/*WLP_SIZE*/
		struct {
			wlp_size_t sz_min;
			wlp_size_t sz_max;
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
	wlp_size_t sz;
	const char* s;
	wlp_strset_t ssi;
} wlp_default_t;

#define WLP_NO_RANGE()				{ B_FALSE, { {0} } }

#define WLP_STRING_LENGTH(length) 	{ B_TRUE, { {length}, {0, 0}, {0.0, 0.0}, {0, 0}, {0, NULL} } }

#define WLP_INT_RANGE(min, max)  	{ B_TRUE, { {0}, {min, max} } }
#define WLP_FLOAT_RANGE(min, max) 	{ B_TRUE, { {0}, {0, 0}, {min, max} } }
#define WLP_SIZE_RANGE(min, max) 	{ B_TRUE, { {0}, {0, 0}, {0.0, 0.0}, {min,  max} } }

#define WLP_STRING_SET_RANGE(set) 	{ B_TRUE, { {0}, {0, 0}, {0.0, 0.0}, {0, 0}, 		\
									    {sizeof((set)) / sizeof(char*), (set) } } }

#define WLP_NO_DEFAULT()				{ B_FALSE, B_FALSE }

#define WLP_BOOLEAN_DEFAULT(b)			{ B_TRUE, b }
#define WLP_INT_DEFAULT(i)				{ B_TRUE, B_FALSE, i }
#define WLP_FLOAT_DEFAULT(f)			{ B_TRUE, B_FALSE, 0, f }
#define WLP_SIZE_DEFAULT(sz)			{ B_TRUE, B_FALSE, 0, 0.0, sz }
#define WLP_STRING_DEFAULT(s)			{ B_TRUE, B_FALSE, 0, 0.0, 0, s }
#define WLP_STRING_SET_DEFAULT(ssi)		{ B_TRUE, B_FALSE, 0, 0.0, 0, NULL, ssi }

#define WLPF_NO_FLAGS				0x00
#define WLPF_OPTIONAL				0x01

/*Description of param*/
typedef struct {
	wlp_type_t type;
	unsigned long flags;

	wlp_range_t range;
	wlp_default_t defval;

	char* name;
	char* description;

	/*Offset in data structure where param value would be written*/
	size_t off;
} wlp_descr_t;

#ifndef NO_JSON
#include <libjson.h>

#define WLPARAM_DEFAULT_OK			0
#define WLPARAM_JSON_OK				0
#define WLPARAM_JSON_WRONG_TYPE		-1
#define WLPARAM_JSON_OUTSIDE_RANGE	-2
#define WLPARAM_JSON_NOT_FOUND		-3
#define WLPARAM_NO_DEFAULT			-4


JSONNODE* json_wlparam_format(wlp_descr_t* wlp);
JSONNODE* json_wlparam_format_all(wlp_descr_t* wlp);
int json_wlparam_proc_all(JSONNODE* node, wlp_descr_t* wlp, void* params);
#endif

#endif /* WLPARAM_H_ */
