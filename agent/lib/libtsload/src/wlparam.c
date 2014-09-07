/*
 * wlparam.c
 *
 *  Created on: Nov 12, 2012
 *      Author: myaut
 */

#define NO_JSON
#define JSONNODE void

#define LOG_SOURCE "wlparam"
#include <log.h>

#include <modules.h>
#include <wlparam.h>
#include <tsload.h>
#include <workload.h>
#include <field.h>

#include <cpuinfo.h>
#include <diskinfo.h>

#include <stdlib.h>
#include <string.h>

#define STRSETCHUNK		256

DECLARE_FIELD_FUNCTIONS(wlp_integer_t);
DECLARE_FIELD_FUNCTIONS(wlp_float_t);
DECLARE_FIELD_FUNCTIONS(wlp_bool_t);
DECLARE_FIELD_FUNCTIONS(wlp_strset_t);
DECLARE_FIELD_FUNCTIONS(wlp_hiobject_t);

const char* wlt_type_names[WLP_TYPE_MAX] = {
	"null",
	"bool",
	"integer",
	"float",
	"string",
	"strset",

	"size",
	"time",
	"filepath",
	"cpuobject",
	"disk"
};

static wlp_type_t wlp_base_types[WLP_TYPE_MAX] = {
	WLP_NULL,
	WLP_BOOL,
	WLP_INTEGER,
	WLP_FLOAT,
	WLP_RAW_STRING,
	WLP_STRING_SET,
	WLP_INTEGER,		/* WLP_SIZE */
	WLP_INTEGER,		/* WLP_SIZE */
	WLP_RAW_STRING,     /* WLP_FILE_PATH */
	WLP_HI_OBJECT, 		/* WLP_CPU_OBJECT */
	WLP_HI_OBJECT, 		/* WLP_DISK */
};

static tsobj_type_t wlp_tsobj_type[WLP_TYPE_MAX] = {
	JSON_NULL,
	JSON_BOOLEAN,				/* WLP_BOOL */
	JSON_NUMBER_INTEGER,		/* WLP_INTEGER */
	JSON_NUMBER_FLOAT,			/* WLP_FLOAT */
	JSON_STRING,				/* WLP_RAW_STRING */
	JSON_STRING,				/* WLP_STRING_SET */
	JSON_NUMBER_INTEGER,		/* WLP_SIZE */
	JSON_NUMBER_INTEGER,		/* WLP_SIZE */
	JSON_STRING,     			/* WLP_FILE_PATH */
	JSON_STRING, 				/* WLP_CPU_OBJECT */
	JSON_STRING, 				/* WLP_DISK */
};

wlp_type_t wlp_get_base_type(wlp_descr_t* wlp) {
	return wlp_base_types[wlp->type];
}

#if 0
static JSONNODE* json_wlparam_strset_format(wlp_descr_t* wlp) {
	JSONNODE* node = json_new(JSON_ARRAY);
	JSONNODE* el;
	int i;
	char* str;

	json_set_name(node, "strset");

	for(i = 0; i < wlp->range.ss_num; ++i) {
		str = wlp->range.ss_strings[i];

		el = json_new(JSON_STRING);
		json_set_a(el, str);

		json_push_back(node, el);
	}

	return node;
}

JSONNODE* json_wlparam_format(wlp_descr_t* wlp) {
	JSONNODE* wlp_node = json_new(JSON_NODE);

	json_set_name(wlp_node, wlp->name);

	json_push_back(wlp_node, json_new_a("type", wlt_type_names[wlp->type]));

	switch(wlp->type) {
	case WLP_BOOL:
		if(wlp->defval.enabled)
			json_push_back(wlp_node, json_new_b("default", wlp->defval.b));
		break;
	case WLP_SIZE:
	case WLP_TIME:
	case WLP_INTEGER:
		if(wlp->range.range) {
			json_push_back(wlp_node, json_new_i("min", wlp->range.i_min));
			json_push_back(wlp_node, json_new_i("max", wlp->range.i_max));
		}
		if(wlp->defval.enabled)
			json_push_back(wlp_node, json_new_i("default", wlp->defval.i));
		break;
	case WLP_FLOAT:
		if(wlp->range.range) {
			json_push_back(wlp_node, json_new_f("min", wlp->range.d_min));
			json_push_back(wlp_node, json_new_f("max", wlp->range.d_max));
		}
		if(wlp->defval.enabled)
			json_push_back(wlp_node, json_new_f("default", wlp->defval.f));
		break;
	case WLP_FILE_PATH:
	case WLP_CPU_OBJECT:
	case WLP_DISK:
	case WLP_RAW_STRING:
		json_push_back(wlp_node, json_new_i("len", wlp->range.str_length));
		if(wlp->defval.enabled)
			json_push_back(wlp_node, json_new_a("default", wlp->defval.s));
		break;
	case WLP_STRING_SET:
		json_push_back(wlp_node, json_wlparam_strset_format(wlp));
		if(wlp->defval.enabled)
			json_push_back(wlp_node, json_new_i("default", wlp->defval.ssi));
		break;
	}

	json_push_back(wlp_node, json_new_i("flags", wlp->flags));
	json_push_back(wlp_node, json_new_a("description", wlp->description));

	return wlp_node;
}

JSONNODE* json_wlparam_format_all(wlp_descr_t* wlp) {
	JSONNODE* node = json_new(JSON_NODE);
	JSONNODE* wlp_node = NULL;

	json_set_name(node, "params");

	while(wlp->type != WLP_NULL) {
		wlp_node = json_wlparam_format(wlp);

		json_push_back(node, wlp_node);
		wlp++;
	}

	return node;
}
#endif

static int tsobj_wlparam_strset_proc(tsobj_node_t* node, wlp_descr_t* wlp, void* param, struct workload* wl) {
	int i;
	const char* str = tsobj_as_string(node);

	for(i = 0; i < wlp->range.ss_num; ++i) {
		if(strcmp(wlp->range.ss_strings[i], str) == 0) {
			FIELD_PUT_VALUE(wlp_strset_t, param, i);
			return WLPARAM_TSOBJ_OK;
		}
	}

	tsload_error_msg(TSE_INVALID_VALUE,
					 WLP_ERROR_PREFIX "string was not found",
					 wl->wl_name, wlp->name);

	return WLPARAM_OUTSIDE_RANGE;
}

static int tsobj_wlparam_hiobj_proc(tsobj_node_t* node, wlp_descr_t* wlp, void* param, struct workload* wl) {
	wlp_hiobject_t hiobj;
	char* str = tsobj_as_string(node);

	switch(wlp->type) {
	case WLP_CPU_OBJECT:
		hiobj = hi_cpu_find(str);
		break;
	case WLP_DISK:
		hiobj = hi_dsk_find(str);
		break;
	}

	if(hiobj == NULL) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 WLP_ERROR_PREFIX "hostinfo object was not found",
						 wl->wl_name, wlp->name);
		return WLPARAM_OUTSIDE_RANGE;
	}

	FIELD_PUT_VALUE(wlp_hiobject_t, param, hiobj);
	return WLPARAM_TSOBJ_OK;
}

int tsobj_wlparam_proc(tsobj_node_t* node, wlp_descr_t* wlp, void* param, struct workload* wl) {
	wlp_range_t* range = &wlp->range;

	/* Check validity of node type. It will raise TSObj errors if needed */
	if(tsobj_check_type(node, wlp_tsobj_type[wlp->type]) != TSOBJ_OK)
		goto bad_tsobj;

	switch(wlp->type) {
	case WLP_BOOL:
		{
			FIELD_PUT_VALUE(wlp_bool_t, param, tsobj_as_boolean(node));
		}
		break;
	case WLP_SIZE:
	case WLP_TIME:
	case WLP_INTEGER:
		{
			wlp_integer_t val = tsobj_as_integer(node);
			if(range->range &&
			   (val < range->i_min || val > range->i_max)) {
					tsload_error_msg(TSE_INVALID_VALUE,
									 WLP_ERROR_PREFIX "outside range [%lld:%lld]",
									 wl->wl_name, wlp->name,
									 (long long) range->i_min,
									 (long long) range->i_max);
					return WLPARAM_OUTSIDE_RANGE;
			}
			FIELD_PUT_VALUE(wlp_integer_t, param, val);
		}
		break;
	case WLP_FLOAT:
		{
			wlp_float_t val = tsobj_as_double(node);
			if(range->range &&
			   (val < range->d_min || val > range->d_max)) {
					tsload_error_msg(TSE_INVALID_VALUE,
									 WLP_ERROR_PREFIX "outside range [%f:%f]",
									 wl->wl_name, wlp->name, range->d_min, range->d_max);
					return WLPARAM_OUTSIDE_RANGE;
			}
			FIELD_PUT_VALUE(wlp_float_t, param, val);
		}
		break;
	case WLP_FILE_PATH:
	case WLP_RAW_STRING:
		{
			const char* str = tsobj_as_string(node);

			if(strlen(str) < range->str_length) {
				strcpy(param, str);
			}
			else {
				tsload_error_msg(TSE_INVALID_VALUE,
								 WLP_ERROR_PREFIX "string is longer than %d",
								 wl->wl_name, wlp->name, range->str_length);
				return WLPARAM_OUTSIDE_RANGE;
			}
		}
		break;
	case WLP_STRING_SET:
		return tsobj_wlparam_strset_proc(node, wlp, param, wl);
	case WLP_CPU_OBJECT:
	case WLP_DISK:
		return tsobj_wlparam_hiobj_proc(node, wlp, param, wl);
	}

	return WLPARAM_TSOBJ_OK;

bad_tsobj:
	tsload_error_msg(tsobj_error_code(), WLP_ERROR_PREFIX "%s",
					 wl->wl_name, wlp->name, tsobj_error_message());

	return WLPARAM_TSOBJ_BAD;
}


int wlparam_set_default(wlp_descr_t* wlp, void* param, struct workload* wl) {
	if(!wlp->defval.enabled) {
		/* There is incosistency between flags and defval attributes of workload parameter descriptor:
		 * parameter is optional, but no default value supplied. Reporting as internal error */
		tsload_error_msg(TSE_INTERNAL_ERROR,
						 WLP_ERROR_PREFIX " parameter is optional, but no default value supplied",
						 wl->wl_name, wlp->name);

		return WLPARAM_NO_DEFAULT;
	}

	switch(wlp->type) {
	case WLP_BOOL:
		FIELD_PUT_VALUE(wlp_bool_t, param, wlp->defval.b);
		break;
	case WLP_SIZE:
	case WLP_TIME:
	case WLP_INTEGER:
		FIELD_PUT_VALUE(wlp_integer_t, param, wlp->defval.i);
		break;
	case WLP_FLOAT:
		FIELD_PUT_VALUE(wlp_float_t, param, wlp->defval.f);
		break;
	case WLP_FILE_PATH:
	case WLP_RAW_STRING:
		/* Not checking length of default value here*/
		strcpy((char*) param, wlp->defval.s);
		break;
	case WLP_STRING_SET:
		FIELD_PUT_VALUE(wlp_strset_t, param, wlp->defval.ssi);
		break;
	}

	return WLPARAM_DEFAULT_OK;
}

int tsobj_wlparam_proc_all(tsobj_node_t* node, wlp_descr_t* wlp, struct workload* wl) {
	int ret = WLPARAM_TSOBJ_OK;
	void* param;
	char* params = wl->wl_params;
	const char* wlp_name = "(null)";

	tsobj_node_t* param_node;

	if(tsobj_check_type(node, JSON_NODE) != TSOBJ_OK)
		goto bad_tsobj;

	while(ret == WLPARAM_TSOBJ_OK && wlp->type != WLP_NULL) {
		param = ((char*) params) + wlp->off;
		wlp_name = wlp->name;

		param_node = tsobj_find(node, wlp->name);

		if((wlp->flags & WLPF_OUTPUT) == WLPF_OUTPUT) {
			if(param_node != NULL) {
				tsload_error_msg(TSE_UNUSED_ATTRIBUTE,
								 WLP_ERROR_PREFIX " it is output parameter",
								 wl->wl_name, wlp->name);
				return WLPARAM_TSOBJ_BAD;
			}
		}
		else {
			if(param_node == NULL) {
				/* If parameter is optional, try to assign it to default value */
				if(!(wlp->flags & WLPF_OPTIONAL))
					goto bad_tsobj;

				if(wlp->flags & WLPF_REQUEST) {
					ret = wlpgen_create_default(wlp, wl);
				}
				else {
					ret = wlparam_set_default(wlp, param, wl);
				}
			}
			else {
				if(wlp->flags & WLPF_REQUEST) {
					ret = tsobj_wlpgen_proc(param_node, wlp, wl);
				}
				else {
					ret = tsobj_wlparam_proc(param_node, wlp, param, wl);
				}
			}
		}

		++wlp;
	}

	if(ret == WLPARAM_TSOBJ_OK && tsobj_check_unused(node) != TSOBJ_OK) {
		wlp_name = "??";
		goto bad_tsobj;
	}

	return ret;

bad_tsobj:
	tsload_error_msg(tsobj_error_code(), WLP_ERROR_PREFIX "%s",
					 wl->wl_name, wlp_name, tsobj_error_message());

	return WLPARAM_TSOBJ_BAD;
}

