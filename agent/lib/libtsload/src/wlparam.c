
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



#define LOG_SOURCE "wlparam"
#include <log.h>

#include <modules.h>
#include <wlparam.h>
#include <tsload.h>
#include <workload.h>
#include <field.h>

#include <cpuinfo.h>
#include <diskinfo.h>

#include <libjson.h>

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

wlp_type_t wlp_get_base_type(wlp_descr_t* wlp) {
	return wlp_base_types[wlp->type];
}

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

int json_wlparam_string_proc(JSONNODE* node, wlp_descr_t* wlp, void* param) {
	wlp_string_t* str = (wlp_string_t*) param;
	char* js = NULL;

	int ret = WLPARAM_JSON_OUTSIDE_RANGE;

	if(json_type(node) != JSON_STRING)
		return WLPARAM_JSON_WRONG_TYPE;

	js = json_as_string(node);

	if(strlen(js) < wlp->range.str_length) {
		strcpy(param, js);
		ret = WLPARAM_JSON_OK;
	}

	json_free(js);

	return ret;
}

static int json_wlparam_strset_proc(JSONNODE* node, wlp_descr_t* wlp, void* param) {
	int i;
	char* js = NULL;

	if(json_type(node) != JSON_STRING)
		return WLPARAM_JSON_WRONG_TYPE;

	js = json_as_string(node);

	for(i = 0; i < wlp->range.ss_num; ++i) {
		if(strcmp(wlp->range.ss_strings[i], js) == 0) {
			FIELD_PUT_VALUE(wlp_strset_t, param, i);

			json_free(js);

			return WLPARAM_JSON_OK;
		}
	}

	/* No match found - wrong value provided*/
	json_free(js);

	FIELD_PUT_VALUE(wlp_strset_t, param, -1);
	return WLPARAM_JSON_OUTSIDE_RANGE;
}

static int json_wlparam_hiobj_proc(JSONNODE* node, wlp_descr_t* wlp, void* param) {
	wlp_hiobject_t hiobj;
	int ret = WLPARAM_JSON_OUTSIDE_RANGE;
	char* js = NULL;

	if(json_type(node) != JSON_STRING)
		return WLPARAM_JSON_WRONG_TYPE;

	js = json_as_string(node);

	switch(wlp->type) {
	case WLP_CPU_OBJECT:
		hiobj =  hi_cpu_find(js);
		break;
	case WLP_DISK:
		hiobj = hi_dsk_find(js);
		break;
	}

	if(hiobj != NULL) {
		ret = WLPARAM_JSON_OK;
	}

	json_free(js);
	FIELD_PUT_VALUE(wlp_hiobject_t, param, hiobj);

	return ret;
}

#define WLPARAM_RANGE_CHECK(min, max)						\
		if(wlp->range.range &&								\
			(value < wlp->range.min ||value > wlp->range.max))	\
					return WLPARAM_JSON_OUTSIDE_RANGE;

#define WLPARAM_NO_RANGE_CHECK

#define WLPARAM_PROC_SIMPLE(type, json_node_type, 	\
		json_as_func, range_check) 					\
	{												\
		type value;									\
		if(json_type(node) != json_node_type)		\
			return WLPARAM_JSON_WRONG_TYPE;			\
		value = json_as_func(node);					\
		range_check;								\
		FIELD_PUT_VALUE(type, param, value);		\
	}

int json_wlparam_proc(JSONNODE* node, wlp_descr_t* wlp, void* param) {
	switch(wlp->type) {
	case WLP_BOOL:
		WLPARAM_PROC_SIMPLE(wlp_bool_t, JSON_BOOL,
				json_as_bool, WLPARAM_NO_RANGE_CHECK);
		break;
	case WLP_SIZE:
	case WLP_TIME:
	case WLP_INTEGER:
		WLPARAM_PROC_SIMPLE(wlp_integer_t, JSON_NUMBER,
				json_as_int, WLPARAM_RANGE_CHECK(i_min, i_max));
		break;
	case WLP_FLOAT:
		WLPARAM_PROC_SIMPLE(wlp_float_t, JSON_NUMBER,
				json_as_float, WLPARAM_RANGE_CHECK(d_min, d_max));
		break;
	case WLP_FILE_PATH:
	case WLP_RAW_STRING:
		return json_wlparam_string_proc(node, wlp, param);
	case WLP_STRING_SET:
		return json_wlparam_strset_proc(node, wlp, param);
	case WLP_CPU_OBJECT:
	case WLP_DISK:
		return json_wlparam_hiobj_proc(node, wlp, param);
	}

	return WLPARAM_JSON_OK;
}

int wlparam_set_default(wlp_descr_t* wlp, void* param) {
	if(!wlp->defval.enabled)
		return WLPARAM_NO_DEFAULT;

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
	case WLP_STRING_SET:
		FIELD_PUT_VALUE(wlp_strset_t, param, wlp->defval.ssi);
		break;
	}

	return WLPARAM_DEFAULT_OK;
}

int json_wlparam_proc_all(JSONNODE* node, wlp_descr_t* wlp, struct workload* wl) {
	int ret;
	void* param;
	char* params = wl->wl_params;

	while(wlp->type != WLP_NULL) {
		JSONNODE_ITERATOR i_param = json_find(node, wlp->name),
						  i_end = json_end(node);
		param = ((char*) params) + wlp->off;

		if((wlp->flags & WLPF_OUTPUT) == WLPF_OUTPUT) {
			if(i_param != i_end) {
				tsload_error_msg(TSE_INTERNAL_ERROR, "Couldn't set output param %s", wlp->name);
				return WLPARAM_INVALID_PARAM;
			}

			++wlp;
			continue;
		}

		if(i_param == i_end) {
			/* If parameter is optional, try to assign it to default value */
			if(wlp->flags & WLPF_OPTIONAL) {
				if(wlp->flags & WLPF_REQUEST) {
					ret = wlpgen_create_default(wlp, wl);
				}
				else {
					ret = wlparam_set_default(wlp, param);
				}

				if(ret == WLPARAM_NO_DEFAULT) {
					tsload_error_msg(TSE_INTERNAL_ERROR, "Missing default value for %s", wlp->name);
					return WLPARAM_JSON_NOT_FOUND;
				}

				wlp++;
				continue;
			}

			tsload_error_msg(TSE_INVALID_DATA, WLP_ERROR_PREFIX " not specified", wlp->name);
			return WLPARAM_JSON_NOT_FOUND;
		}

		if(wlp->flags & WLPF_REQUEST) {
			ret = json_wlpgen_proc(*i_param, wlp, wl);
		}
		else {
			ret = json_wlparam_proc(*i_param, wlp, param);
		}

		if(ret == WLPARAM_JSON_WRONG_TYPE) {
			tsload_error_msg(TSE_INVALID_DATA, WLP_ERROR_PREFIX " has wrong type", wlp->name);
			return ret;
		}
		else if(ret == WLPARAM_JSON_OUTSIDE_RANGE) {
			tsload_error_msg(TSE_INVALID_DATA, WLP_ERROR_PREFIX " outside defined range", wlp->name);
			return ret;
		}
		else if(ret != WLPARAM_JSON_OK) {
			tsload_error_msg(TSE_INTERNAL_ERROR, WLP_ERROR_PREFIX ": internal error", wlp->name);
			return ret;
		}

		++wlp;
	}

	return WLPARAM_JSON_OK;
}

