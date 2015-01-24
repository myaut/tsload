/*
    This file is part of TSLoad.
    Copyright 2015, Sergey Klyaus, Tune-IT

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

#include <tsload/mempool.h>

#include <tsload/load/wlparam.h>

#include <tsload/json/json.h>

#include <genmodsrc.h>

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

/* Should be sync with lib/libtsload/wlparam.c */
const char* wlt_type_tsobj_classes[WLP_TYPE_MAX] = {
	"tsload.wlparam.WLParamNull",
	"tsload.wlparam.WLParamBoolean",
	"tsload.wlparam.WLParamInteger",
	"tsload.wlparam.WLParamFloat",
	"tsload.wlparam.WLParamString",
	"tsload.wlparam.WLParamStringSet",

	"tsload.wlparam.WLParamSize",
	"tsload.wlparam.WLParamTime",
	"tsload.wlparam.WLParamFilePath",
	"tsload.wlparam.WLParamCPUObject",
	"tsload.wlparam.WLParamDisk"
};

static const char* wlp_type_names[WLP_TYPE_MAX] = {
	"",						/* WLP_NULL */
	"wlp_bool_t",			/* WLP_BOOLEAN */
	"wlp_integer_t",		/* WLP_INTEGER */
	"wlp_float_t",			/* WLP_FLOAT */
	"char",					/* WLP_RAW_STRING */
	"wlp_strset_t",			/* WLP_STRING_SET */
	"wlp_integer_t",		/* WLP_SIZE */
	"wlp_integer_t",		/* WLP_TIME */
	"char",     			/* WLP_FILE_PATH */
	"wlp_hiobject_t", 		/* WLP_CPU_OBJECT */
	"wlp_hiobject_t", 		/* WLP_DISK */
};

static const char* wlp_enum_names[WLP_TYPE_MAX] = {
	"WLP_NULL",
	"WLP_BOOL",
	"WLP_INTEGER",
	"WLP_FLOAT",
	"WLP_RAW_STRING",
	"WLP_STRING_SET",
	"WLP_SIZE",
	"WLP_TIME",
	"WLP_FILE_PATH",
	"WLP_CPU_OBJECT",
	"WLP_DISK"
};

struct modpp_wlt_class_name {
	const char* literal;
	const char* name;
} modpp_wlt_class_names[] = {
	{"WLC_CPU_INTEGER", "cpu_integer"},
	{"WLC_CPU_FLOAT", 	 "cpu_float"},
	{"WLC_CPU_MEMORY",   "cpu_memory"},
	{"WLC_CPU_MISC",      "cpu_misc"},

	{"WLC_MEMORY_ALLOCATION ",  "mem_allocation"},

	{"WLC_FILESYSTEM_OP",  "fs_op"},
	{"WLC_FILESYSTEM_RW",  "fs_rw"},

	{"WLC_DISK_RW",  "disk_rw"},

	{"WLC_NETWORK",  "network"},

	{"WLC_OS_BENCHMARK",  "os"},
	{NULL, NULL}
};


void modinfo_gen_wlparam_fields(FILE* outf, wlp_descr_t* params, boolean_t is_request) {
	wlp_descr_t* param = params;

	while(param->type != WLP_NULL) {
		if(TO_BOOLEAN(param->flags & WLPF_REQUEST) != is_request) {
			++param;
			continue;
		}

		fputc('\t', outf);
		fputs(wlp_type_names[param->type], outf);
		fputc(' ', outf);
		fputs(param->name, outf);

		if(param->type == WLP_RAW_STRING || param->type == WLP_FILE_PATH) {
			fprintf(outf, "[%d]", param->range.str_length);
		}

		fputc(';', outf);
		fputc('\n', outf);

		++param;
	}
}

void modinfo_gen_wlparam_fields_wl(FILE* outf, void* obj) {
	wlp_descr_t* params = (wlp_descr_t*) obj;

	return modinfo_gen_wlparam_fields(outf, params, B_FALSE);
}

void modinfo_gen_wlparam_fields_rq(FILE* outf, void* obj) {
	wlp_descr_t* params = (wlp_descr_t*) obj;

	return modinfo_gen_wlparam_fields(outf, params, B_TRUE);
}

void modinfo_gen_wlparam_strset(FILE* outf, void* obj) {
	wlp_descr_t* params = (wlp_descr_t*) obj;
	wlp_descr_t* param = params;

	int i;
	int count = param->range.ss_num;

	while(param->type != WLP_NULL) {
		if(param->type == WLP_STRING_SET) {
			fprintf(outf, "const char* %s_strset[] = {", param->name);

			for(i = 0; i < count; ++i) {
				fprintf(outf, "\"%s\"", param->range.ss_strings[i]);

				if(i < (count - 1))
					fputs(", ", outf);
			}

			fputs("};\n", outf);
		}

		++param;
	}
}

#define WLPLINE(line) ("\t  " line ",\n")

void modinfo_gen_wlparam_array(FILE* outf, void* obj) {
	wlp_descr_t* params = (wlp_descr_t*) obj;
	wlp_descr_t* param = params;
	boolean_t has_flag;

	const char* wl_struct_name = modvar_get_string("WL_PARAM_STRUCT");
	const char* rq_struct_name = modvar_get_string("RQ_PARAM_STRUCT");

	while(param->type != WLP_NULL) {
		fprintf(outf, "\t{ %s, ", wlp_enum_names[param->type]);

		/* Write flags to outf - use intermediate flag has_flag
		 * to know if another flag was set previously and | should be added */
		has_flag = B_FALSE;
		if((param->flags & WLPF_OUTPUT) == WLPF_OUTPUT) {
			fputs("WLPF_OUTPUT", outf);
			has_flag = B_TRUE;
		}
		else if(param->flags & WLPF_REQUEST) {
			fputs("WLPF_REQUEST", outf);
			has_flag = B_TRUE;
		}
		if(param->flags & WLPF_OPTIONAL) {
			if(has_flag)
				fputs(" | ", outf);
			fputs("WLPF_OPTIONAL", outf);
			has_flag = B_TRUE;
		}
		if(!has_flag) {
			fputs("WLPF_NO_FLAGS", outf);
		}
		fputs(", \n", outf);

		if(param->range.range) {
			switch(param->type) {
			case WLP_SIZE:
			case WLP_TIME:
			case WLP_INTEGER:
				fprintf(outf, WLPLINE("WLP_INT_RANGE(%" PRId64 ", %" PRId64 ")"),
						param->range.i_min, param->range.i_max);
				break;
			case WLP_FLOAT:
				fprintf(outf, WLPLINE("WLP_FLOAT_RANGE(%f, %f)"), param->range.d_min, param->range.d_max);
				break;
			case WLP_FILE_PATH:
			case WLP_RAW_STRING:
				fprintf(outf, WLPLINE("WLP_STRING_LENGTH(%d)"), param->range.str_length);
				break;
			case WLP_STRING_SET:
				fprintf(outf, WLPLINE("WLP_STRING_SET_RANGE(%s_strset)"), param->name);
				break;
			}
		}
		else {
			fputs(WLPLINE("WLP_NO_RANGE()"), outf);
		}

		if(param->defval.enabled) {
			switch(param->type) {
				case WLP_BOOL:
					fprintf(outf, WLPLINE("WLP_BOOLEAN_DEFAULT(%s)"),
							(param->defval.b)? "B_TRUE" : "B_FALSE");
					break;
				case WLP_SIZE:
				case WLP_TIME:
				case WLP_INTEGER:
					fprintf(outf, WLPLINE("WLP_INT_DEFAULT(%" PRId64 ")"), param->defval.i);
					break;
				case WLP_FLOAT:
					fprintf(outf, WLPLINE("WLP_FLOAT_DEFAULT(%f)"), param->defval.f);
					break;
				case WLP_FILE_PATH:
				case WLP_RAW_STRING:
				case WLP_CPU_OBJECT:
				case WLP_DISK:
					fprintf(outf, WLPLINE("WLP_STRING_DEFAULT(\"%s\")"), param->defval.s);
					break;
				case WLP_STRING_SET:
					fprintf(outf, WLPLINE("WLP_STRING_SET_DEFAULT(%d)"), param->defval.ssi);
					break;
			}
		}
		else {
			fputs(WLPLINE("WLP_NO_DEFAULT()"), outf);
		}

		fprintf(outf, WLPLINE("\"%s\""), param->name);
		fprintf(outf, WLPLINE("\"%s\""), param->description);

		fprintf(outf, WLPLINE("offsetof(struct %s, %s) }"),
				(param->flags & WLPF_REQUEST) ? rq_struct_name : wl_struct_name,
				param->name);

		++param;
	}
}

void modinfo_wlparam_set_gen(wlp_descr_t* params) {
	wlp_descr_t* param = params;
	boolean_t has_wl_params = B_FALSE;
	boolean_t has_rq_params = B_FALSE;

	/* Check if there are only workload or request params */
	while(param->type != WLP_NULL) {
		if(param->flags & WLPF_REQUEST) {
			has_rq_params = B_TRUE;
		}
		else {
			has_wl_params = B_TRUE;
		}

		if(has_rq_params && has_wl_params)
			break;

		++param;
	}

	/* Disable variables if needed */
	if(has_wl_params) {
		modvar_set_gen(modvar_create("WL_PARAM_FIELDS"),
					   modinfo_gen_wlparam_fields_wl,
					   NULL, params);
	}
	else {
		modvar_unset("WL_PARAM_STRUCT");
		modvar_unset("WL_PARAM_VARNAME");
	}

	if(has_rq_params) {
		modvar_set_gen(modvar_create("RQ_PARAM_FIELDS"),
					   modinfo_gen_wlparam_fields_rq,
					   NULL, params);
	}
	else {
		modvar_unset("RQ_PARAM_STRUCT");
		modvar_unset("RQ_PARAM_VARNAME");
	}

	modvar_set_gen(modvar_create("WL_PARAMS_STRSET"),
				   modinfo_gen_wlparam_strset,
				   NULL, params);
	modvar_set_gen(modvar_create("WL_PARAMS_ARRAY"),
				   modinfo_gen_wlparam_array,
				   NULL, params);
}

int modinfo_parse_wlp_strset(wlp_descr_t* param, json_node_t* j_param) {
	json_node_t* j_strset;
	json_node_t* j_ss_val;
	const char* value;
	int ssi;

	if(json_get_array(j_param, "strset", &j_strset) != JSON_OK)
		return MODINFO_CFG_INVALID_PARAM;

	param->range.ss_num = json_size(j_strset);
	param->range.ss_strings = mp_malloc(sizeof(char*) * json_size(j_strset));
	param->range.range = B_TRUE;

	json_for_each(j_strset, j_ss_val, ssi) {
		value = json_as_string(j_ss_val);
		if(value == NULL)
			return MODINFO_CFG_INVALID_PARAM;

		param->range.ss_strings[ssi] = value;
	}

	return MODINFO_OK;
}

#define MODINFO_PARSE_WLPFLAG(name, flag, mask)					\
		err = json_get_boolean(j_param, name, &flag);			\
		if(err == JSON_OK) {									\
			if(flag) {											\
				param->flags |= mask; }	}						\
		else if(err != JSON_NOT_FOUND) {						\
			goto bad_json; }

int modinfo_parse_wlparam(wlp_descr_t* param, json_node_t* j_param) {
	char* type;
	int wlpt;
	int err_defval = JSON_NOT_FOUND, err_min = JSON_NOT_FOUND,
		err_max = JSON_NOT_FOUND, err = JSON_OK;
	boolean_t request = B_FALSE, optional = B_FALSE, output = B_FALSE;

	param->type = WLP_NULL;
	param->name = json_name(j_param);

	/* Parse _type using wlt_type_tsobj_classes[] */
	if(json_get_string(j_param, "_type", &type) != JSON_OK)
		goto bad_json;

	for(wlpt = 0; wlpt < WLP_TYPE_MAX; ++wlpt) {
		if(strcmp(type, wlt_type_tsobj_classes[wlpt]) == 0) {
			param->type = (wlp_type_t) wlpt;
			break;
		}
	}
	if(param->type == WLP_NULL) {
		fprintf(stderr, "Failed to parse modinfo param '%s': invalid type '%s'\n",
				param->name, type);
		return MODINFO_CFG_INVALID_PARAM;
	}

	/* Parse description */
	param->description = NULL;
	if(json_get_string(j_param, "description", &param->description) != JSON_OK)
		goto bad_json;

	/* Parse default value/range */
	param->range.range = B_FALSE;
	param->defval.enabled = B_FALSE;
	switch(param->type) {
		case WLP_BOOL:
			err_defval = json_get_boolean(j_param, "default", &param->defval.b);
			break;
		case WLP_SIZE:
		case WLP_TIME:
		case WLP_INTEGER:
			err_min = json_get_integer_i64(j_param, "min", &param->range.i_min);
			err_max = json_get_integer_i64(j_param, "max", &param->range.i_max);
			err_defval = json_get_integer_i64(j_param, "default", &param->defval.i);
			break;
		case WLP_FLOAT:
			err_min = json_get_double(j_param, "min", &param->range.d_min);
			err_max = json_get_double(j_param, "max", &param->range.d_max);
			err_defval = json_get_double(j_param, "default", &param->defval.f);
			break;
		case WLP_FILE_PATH:
		case WLP_RAW_STRING:
			if(json_get_integer_u(j_param, "len", &param->range.str_length) != JSON_OK)
				goto bad_json;
			param->range.range = B_TRUE;
			/* FALLTHROUGH */
		case WLP_CPU_OBJECT:
		case WLP_DISK:
			err_defval = json_get_string(j_param, "default", &param->defval.s);
			break;
		case WLP_STRING_SET:
			err = modinfo_parse_wlp_strset(param, j_param);
			if(err != MODINFO_OK)
				goto bad_json;

			err_defval = json_get_integer_i(j_param, "default", &param->defval.ssi);
			break;
	}

	if(err_defval == JSON_OK) {
		param->defval.enabled = B_TRUE;
	}
	else if(err_defval != JSON_NOT_FOUND) {
		goto bad_json;
	}

	if(err_min == JSON_OK && err_max == JSON_OK) {
		param->range.range = B_TRUE;
	}
	else if(!(err_min == JSON_NOT_FOUND && err_max == JSON_NOT_FOUND)) {
		goto bad_json;
	}

	/* Parse flags */
	param->flags = 0;
	MODINFO_PARSE_WLPFLAG("request", request, WLPF_REQUEST);
	MODINFO_PARSE_WLPFLAG("output", output, WLPF_OUTPUT);
	MODINFO_PARSE_WLPFLAG("optional", optional, WLPF_OPTIONAL);

	return MODINFO_OK;

bad_json:
	fprintf(stderr, "Failed to parse modinfo param '%s': %s\n",
			param->name, json_error_message());
	return MODINFO_CFG_INVALID_PARAM;
}

wlp_descr_t* modinfo_create_wlparams(size_t param_count) {
	wlp_descr_t* params;
	int paramid;

	++param_count;

	params = mp_malloc(param_count * sizeof(wlp_descr_t));
	for(paramid = 0; paramid < param_count; ++paramid) {
		params[paramid].type = WLP_NULL;
	}

	return params;
}

void modinfo_destroy_wlparams(void* obj) {
	wlp_descr_t* params = (wlp_descr_t*) obj;
	wlp_descr_t* param = params;

	while(param->type != WLP_NULL) {
		if(param->type == WLP_STRING_SET && param->range.range) {
			mp_free(param->range.ss_strings);
		}

		++param;
	}

	mp_free(params);
}

int modinfo_parse_wlparams(json_node_t* root) {
	json_node_t* j_params;
	json_node_t* j_param;
	wlp_descr_t* params;
	wlp_descr_t* param;

	size_t param_count;
	int paramid;

	int ret = MODINFO_OK;

	if(json_get_node(root, "params", &j_params) != JSON_OK) {
		fprintf(stderr, "Failed to parse modinfo: %s\n", json_error_message());

		return MODINFO_CFG_NO_PARAMS;
	}

	param_count = json_size(j_params);
	params = modinfo_create_wlparams(param_count);

	modvar_set_gen(modvar_create("_WL_PARAMS"), NULL,
				   modinfo_destroy_wlparams, params);

	param = params;
	json_for_each(j_params, j_param, paramid) {
		ret = modinfo_parse_wlparam(param, j_param);
		if(ret != MODINFO_OK)
			return ret;

		++param;
	}

	modinfo_wlparam_set_gen(params);

	return ret;
}

int modinfo_parse_wlt_class(json_node_t* wltype) {
	json_node_t* j_wlt_class_array;
	json_node_t* j_wlt_class;
	size_t wltc_count, str_len;
	int wlti;
	const char** wlt_class_array = NULL;
	const char* wlt_class_name = NULL;
	char* wlt_class_string = NULL;
	struct modpp_wlt_class_name* wltc;

	int ret = MODINFO_OK;

	/* Parse workload class. Read array of string names of wlt classes, that share
	 * names with wlt_class_names from libtsload/wltypes.c (they are used in
	 * wlt list in JSON format), resolves corresponding C literals and builds
	 * array wlt_class_array from that.
	 *
	 * In the end, it joins wlt_class_name into wlt_class_string with " | " as
	 * delimiter.
	 */

	if(json_get_array(wltype, "class", &j_wlt_class_array) != JSON_OK) {
		fprintf(stderr, "Failed to parse workload type '%s': %s\n", 
				json_safe_name(wltype), json_error_message());

		return MODINFO_CFG_NO_WLT_CLASS;
	}

	wltc_count = json_size(j_wlt_class_array);
	if(wltc_count == 0) {
		fprintf(stderr, "Failed to parse workload type '%s': empty wlt_class\n", 
				json_safe_name(wltype));
		return MODINFO_CFG_NO_WLT_CLASS;
	}

	wlt_class_array = mp_malloc(sizeof(const char*) * wltc_count);
	str_len = 0;

	json_for_each(j_wlt_class_array, j_wlt_class, wlti) {
		wlt_class_name = json_as_string(j_wlt_class);

		if(wlt_class_name == NULL) {
			fprintf(stderr, "Failed to workload type '%s': modinfo: %s\n", 
					json_safe_name(wltype), json_error_message());
			ret = MODINFO_CFG_INVAL_WLT_CLASS;
			goto end;
		}

		wltc = modpp_wlt_class_names;
		while(wltc->literal != NULL) {
			if(strcmp(wltc->name, wlt_class_name) == 0) {
				wlt_class_array[wlti] = wltc->literal;
				str_len += strlen(wltc->literal) + 3;
				break;
			}

			++wltc;
		}
		if(wltc->literal == NULL) {
			fprintf(stderr, "Failed to parse workload type '%s': unknown workload class '%s'\n", 
					json_safe_name(wltype), wlt_class_name);
			ret = MODINFO_CFG_INVAL_WLT_CLASS;
			goto end;
		}
	}

	wlt_class_string = mp_malloc(str_len);

	strcpy(wlt_class_string, wlt_class_array[0]);
	for(wlti = 1; wlti < wltc_count; ++wlti) {
		strcat(wlt_class_string, " | ");
		strcat(wlt_class_string, wlt_class_array[wlti]);
	}

	modvar_set(modvar_create("WLT_CLASS"), wlt_class_string);

end:
	mp_free(wlt_class_string);
	mp_free(wlt_class_array);

	return ret;
}