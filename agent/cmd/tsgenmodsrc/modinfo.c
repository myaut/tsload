/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, Tune-IT

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

#include <tsload/pathutil.h>
#include <tsload/dirent.h>
#include <tsload/mempool.h>

#include <tsload/json/json.h>

#include <genmodsrc.h>

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

/*
 * Parameters:
 *  * MODNAME
 *  * WLT_NAME
 *  * WLT_ABBR
 *  * SOURCE_FILENAME
 *  * HEADER_FILENAME
 * 	* HEADER_DEF
 * 	* WL_PARAM_STRUCT, WL_PARAM_FIELDS, WL_PARAM_VARNAME
 * 	* WL_DATA_STRUCT, WL_DATA_VARNAME
 * 	* RQ_PARAM_STRUCT, RQ_PARAM_FIELDS, RQ_PARAM_VARNAME
 * 	* WLT_VARNAME
 * 	* WLT_CLASS
 * 	* WL_PARAMS_VARNAME
 * 	* WL_PARAMS_ARRAY
 *  * FUNC_CONFIG
 *  * FUNC_UNCONFIG
 *  * FUNC_RUN_REQUEST
 *  * FUNC_STEP
 */


/* Checks that directory containing modinfo is clear except for
 * the modinfo.json itself */
int modinfo_check_dir(const char* modinfo_path) {
	path_split_iter_t iter;
	const char* modinfo_dir;
	const char* modinfo_filename;

	plat_dir_t* dir;
	plat_dir_entry_t* d = NULL;

	int ret = MODINFO_OK;

	modinfo_filename = path_split(&iter, -2, modinfo_path);
	modinfo_dir = path_split_next(&iter);

	dir = plat_opendir(modinfo_dir);

	if(!dir) {
		fprintf(stderr, "Failed to check modinfo: cannot open directory '%s'\n", modinfo_dir);
		return MODINFO_DIR_ERROR;
	}

	while((d = plat_readdir(dir)) != NULL) {
		switch(plat_dirent_type(d)) {
		case DET_REG:
			if(strcmp(d->d_name, modinfo_filename) == 0) {
				continue;
			}
			break;
		case DET_PARENT_DIR:
		case DET_CURRENT_DIR:
			continue;
		}

		fprintf(stderr, "Failed to check modinfo: directory '%s' isn't empty, "
				"found file '%s'\n", modinfo_dir, d->d_name);

		ret = MODINFO_DIR_NOT_EMPTY;
		break;
	}

	plat_closedir(dir);

	return ret;
}

/**
 * Read variable from incoming JSON config
 *  - If it is not set there, sprintf predefined value from fmtstr
 *  - If it is not string, return NULL
 *  - If it is set in JSON config, use value from that
 *
 * Returns final value
 */
const char* modinfo_read_var(json_node_t* vars, modvar_t* var, const char* fmtstr, ...) {
	char* value;
	int err = JSON_NOT_FOUND;

	if(var == NULL) {
		return NULL;
	}

	if(vars != NULL) {
		err = json_get_string(vars, var->name, &value);
	}

	if(likely(err == JSON_NOT_FOUND)) {
		va_list va;

		va_start(va, fmtstr);
		modvar_vprintf(var, fmtstr, va);
		va_end(va);
	}
	else if(unlikely(err == JSON_OK)) {
		if(strlen(value) == 0) {
			/* Empty strings are not allowed here */
			return NULL;
		}

		modvar_set(var, value);
	}
	else {
		return NULL;
	}

	return var->value;
}

static char* modinfo_wlt_abbr(const char* wlt_name) {
	/* Abbreviate workload type name so request variables will get
	 * short but meaningful names, i.e. busy_wait -> bw
	 * Use first letter for each part before underscore + first_letter */

	/* Size to allocate: 1 for first letter, 1 for null-term, and
	 * no more than half of letters in wlt_name (we ignore underscores) */
	char* wlt_abbr = mp_malloc(strlen(wlt_name) / 2 + 2);
	char* p = wlt_abbr;
	const char* q = wlt_name;

	/* Underscore flag, nothing related to stripes & stars
	 * Set to true so initially set first letter */
	boolean_t usflag = B_TRUE;

	while(*q) {
		if(*q == '_') {
			usflag = B_TRUE;
		}
		else if(usflag) {
			*p++ = *q++;
			usflag = B_FALSE;
		}

		++q;
	}

	*p = '\0';

	return wlt_abbr;
}

static char* modinfo_header_def(const char* header_name) {
	char* header_def = mp_malloc(strlen(header_name) + 2);
	char* p = header_def;
	const char* q = header_name;

	while(*q) {
		if(isalpha(*q)) {
			*p++ = toupper(*q);
		}
		else {
			*p++ = '_';
		}

		q++;
	}

	*p++ = '_';
	*p = '\0';

	return header_def;
}

#define MODINFO_READ_VAR(vars, name, fmtstr, ... )				\
	if(modinfo_read_var(vars, modvar_create(name),				\
						fmtstr, __VA_ARGS__) == NULL) 			\
		goto end;
#define MODINFO_READ_VAR2(vars, dest, name, fmtstr, ... )		\
	if((dest = modinfo_read_var(vars, modvar_create(name), 		\
						fmtstr, __VA_ARGS__)) == NULL) 			\
		goto end;

int modinfo_read_vars(const char* modname, json_node_t* vars) {
	const char* wlt_name = NULL;
	const char* header_name = NULL;
	const char* wlt_abbr = NULL;
	const char* header_def = NULL;

	char* wlt_abbr_tmp = NULL;
	char* header_def_tmp = NULL;

	int ret = MODINFO_CFG_INVALID_VAR;

	MODINFO_READ_VAR2(vars, wlt_name, "WLT_NAME", "%s", modname);

	wlt_abbr_tmp = modinfo_wlt_abbr(wlt_name);
	MODINFO_READ_VAR2(vars, wlt_abbr, "WLT_ABBR", "%s", wlt_abbr_tmp);

	MODINFO_READ_VAR(vars, "SOURCE_FILENAME", "%s.c", modname);
	MODINFO_READ_VAR2(vars, header_name, "HEADER_FILENAME", "%s.h", modname);

	header_def_tmp = modinfo_header_def(header_name);
	MODINFO_READ_VAR2(vars, header_def, "HEADER_DEF", "%s", header_def_tmp);

	MODINFO_READ_VAR(vars, "WL_PARAM_STRUCT", "%s_workload", wlt_name);
	MODINFO_READ_VAR(vars, "WL_PARAM_VARNAME", "%swl", wlt_abbr);

	/* FIXME: Read only if data structure is needed */
	MODINFO_READ_VAR(vars, "WL_DATA_STRUCT", "%s_data", wlt_name);
	MODINFO_READ_VAR(vars, "WL_DATA_VARNAME", "%sdata", wlt_abbr);

	MODINFO_READ_VAR(vars, "RQ_PARAM_STRUCT", "%s_request", wlt_name);
	MODINFO_READ_VAR(vars, "RQ_PARAM_VARNAME", "%srq", wlt_abbr);

	/* TODO: WLT_CLASS */
	modvar_set(modvar_create("WLT_CLASS"), "@WLT_CLASS@");

	MODINFO_READ_VAR(vars, "WLT_VARNAME", "%s_wlt", wlt_name);
	MODINFO_READ_VAR(vars, "WL_PARAMS_VARNAME", "%s_params", wlt_name);

	MODINFO_READ_VAR(vars, "FUNC_CONFIG", "%s_wl_config", wlt_name);
	MODINFO_READ_VAR(vars, "FUNC_UNCONFIG", "%s_wl_unconfig", wlt_name);
	MODINFO_READ_VAR(vars, "FUNC_RUN_REQUEST", "%s_run_request", wlt_name);

	/* FIXME: Read only if step function is needed */
	MODINFO_READ_VAR(vars, "FUNC_STEP", "%s_step", wlt_name);

	/* TODO: Implement real params */
	modvar_set(modvar_create("WL_PARAM_FIELDS"), "@WL_PARAM_FIELDS@");
	/* modvar_set(modvar_create("RQ_PARAM_FIELDS"), "@RQ_PARAM_FIELDS@"); */
	modvar_set(modvar_create("WL_PARAMS_ARRAY"), "@WL_PARAMS_ARRAY@");

	ret = MODINFO_OK;

end:
	if(ret != MODINFO_OK) {
		if(json_errno() != JSON_OK) {
			fprintf(stderr, "Failed to read variables: %s\n", json_error_message());
		}
		else {
			fprintf(stderr, "Failed to read variables: internal error (possibly empty string?)\n");
		}
	}

	mp_free(wlt_abbr_tmp);
	mp_free(header_def_tmp);

	return ret;
}

int modinfo_read_config(const char* modinfo_path) {
	json_node_t* root;
	json_node_t* vars = NULL;

	char* modname = NULL;

	int ret = MODINFO_OK;

	json_buffer_t* buf = json_buf_from_file(modinfo_path);

	if(buf == NULL) {
		fprintf(stderr, "Failed to open modinfo: %s\n", json_error_message());
		return MODINFO_CFG_CANNOT_OPEN;
	}

	if(json_parse(buf, &root) != JSON_OK) {
		fprintf(stderr, "Failed to parse modinfo: %s\n", json_error_message());
		return MODINFO_CFG_PARSE_ERROR;
	}

	/* Read name */
	if(json_get_string(root, "name", &modname) != JSON_OK) {
		fprintf(stderr, "%s", json_error_message());
		ret = MODINFO_CFG_MISSING_NAME;

		goto end;
	}
	modvar_set(modvar_create("MODNAME"), modname);

	if(json_get_node(root, "vars", &vars) == JSON_INVALID_TYPE) {
		fprintf(stderr, "Failed to parse modinfo: %s\n", json_error_message());

		ret = MODINFO_CFG_INVALID_VAR;
		goto end;
	}
	modinfo_read_vars(modname, vars);

end:
	json_node_destroy(root);
	return ret;
}
