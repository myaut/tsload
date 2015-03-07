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

#include <tsload/load/wlparam.h>

#include <tsload/json/json.h>

#include <genmodsrc.h>

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

extern const char* var_namespace;

/* Checks that directory containing modinfo is clear except for
 * the modinfo.json itself */
int modinfo_check_dir(const char* modinfo_dir, boolean_t silent) {
	plat_dir_t* dir;
	plat_dir_entry_t* d = NULL;

	int ret = MODINFO_OK;

	dir = plat_opendir(modinfo_dir);

	if(!dir) {
		if(!silent)
			fprintf(stderr, "Failed to check modinfo: cannot open directory '%s'\n", modinfo_dir);
		return MODINFO_DIR_ERROR;
	}

	while((d = plat_readdir(dir)) != NULL) {
		switch(plat_dirent_type(d)) {
		case DET_REG:
			if(path_cmp(d->d_name, MODINFO_FILENAME) == 0) {
				continue;
			}
			if(path_cmp(d->d_name, CTIME_CACHE_FN) == 0) {
				continue;
			}
			break;
		case DET_PARENT_DIR:
		case DET_CURRENT_DIR:
			continue;
		}

		if(!silent) {
			fprintf(stderr, "Failed to check modinfo: directory '%s' isn't empty, "
					"found file '%s'\n", modinfo_dir, d->d_name);
		}

		ret = MODINFO_DIR_NOT_EMPTY;
		break;
	}

	plat_closedir(dir);

	modvar_set(modvar_create("MODINFODIR"), modinfo_dir);

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

	json_errno_clear();
	
	if(var == NULL) {
		return NULL;
	}

	if(vars != NULL) {
		err = json_get_string(vars, var->v_name.name, &value);
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

	return var->v_value.str;
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

void modinfo_error_readvars(void) {
	if(json_errno() != JSON_OK) {
		fprintf(stderr, "Failed to read variables: %s\n", json_error_message());
	}
	else {
		fprintf(stderr, "Failed to read variables: internal error (possibly empty string?)\n");
	}
}
	
int modinfo_read_vars(const char* modname, json_node_t* root) {
	json_node_t* vars = NULL;
	
	const char* header_name = NULL;
	const char* header_def = NULL;

	char* header_def_tmp = NULL;

	int ret = MODINFO_CFG_INVALID_VAR;
	
	if(json_get_node(root, "vars", &vars) == JSON_INVALID_TYPE)
		goto end;
	
	/* Global modinfo vars */
	MODINFO_READ_VAR(vars, "SOURCE_FILENAME", "%s.c", modname);
	MODINFO_READ_VAR2(vars, header_name, "HEADER_FILENAME", "%s.h", modname);
	MODINFO_READ_VAR(vars, "OBJECT_FILENAME", "%s%s", modname,
			modvar_get_string("SHOBJSUFFIX"));
	MODINFO_READ_VAR(vars, "MODULE_FILENAME", "%s%s", modname,
			modvar_get_string("SHLIBSUFFIX"));

	MODINFO_READ_VAR(vars, "MAKEFILE_FILENAME", "%s", "Makefile");
	MODINFO_READ_VAR(vars, "SCONSCRIPT_FILENAME", "%s", "SConscript");
	MODINFO_READ_VAR(vars, "SCONSTRUCT_FILENAME", "%s", "SConstruct");

	header_def_tmp = modinfo_header_def(header_name);
	MODINFO_READ_VAR2(vars, header_def, "HEADER_DEF", "%s", header_def_tmp);

	ret = MODINFO_OK;

end:
	if(ret != MODINFO_OK)
		modinfo_error_readvars();

	mp_free(header_def_tmp);

	return ret;
}

int modinfo_read_vars_wltype(const char* wltname, json_node_t* root) {
	json_node_t* vars = NULL;
	
	const char* wlt_name = NULL;	
	const char* wlt_abbr = NULL;
	
	char* wlt_abbr_tmp = NULL;
	
	int ret = MODINFO_CFG_INVALID_VAR;
	
	if(json_get_node(root, "vars", &vars) == JSON_INVALID_TYPE)
		goto end;
	
	MODINFO_READ_VAR2(vars, wlt_name, "WLT_NAME", "%s", wltname);

	wlt_abbr_tmp = modinfo_wlt_abbr(wlt_name);
	MODINFO_READ_VAR2(vars, wlt_abbr, "WLT_ABBR", "%s", wlt_abbr_tmp);

	MODINFO_READ_VAR(vars, "WL_PARAM_STRUCT", "%s_workload", wlt_name);
	MODINFO_READ_VAR(vars, "WL_PARAM_VARNAME", "%swl", wlt_abbr);

	if(modvar_is_set("HAS_DATA")) {
		MODINFO_READ_VAR(vars, "WL_DATA_STRUCT", "%s_data", wlt_name);
		MODINFO_READ_VAR(vars, "WL_DATA_VARNAME", "%sdata", wlt_abbr);
	}

	MODINFO_READ_VAR(vars, "RQ_PARAM_STRUCT", "%s_request", wlt_name);
	MODINFO_READ_VAR(vars, "RQ_PARAM_VARNAME", "%srq", wlt_abbr);

	MODINFO_READ_VAR(vars, "WLT_VARNAME", "%s_wlt", wlt_name);
	MODINFO_READ_VAR(vars, "WL_PARAMS_VARNAME", "%s_params", wlt_name);

	MODINFO_READ_VAR(vars, "FUNC_CONFIG", "%s_wl_config", wlt_name);
	MODINFO_READ_VAR(vars, "FUNC_UNCONFIG", "%s_wl_unconfig", wlt_name);
	MODINFO_READ_VAR(vars, "FUNC_RUN_REQUEST", "%s_run_request", wlt_name);

	if(modvar_is_set("HAS_STEP")) {
		MODINFO_READ_VAR(vars, "FUNC_STEP", "%s_step", wlt_name);
	}
	
	ret = MODINFO_OK;

end:
	if(ret != MODINFO_OK)
		modinfo_error_readvars();
	
	mp_free(wlt_abbr_tmp);
	
	return ret;	
}

int modinfo_proc_bool(json_node_t* node, const char* param_name, const char* var_name) {
	boolean_t bval;
	
	if(json_get_boolean(node, param_name, &bval) == JSON_INVALID_TYPE) {
		fprintf(stderr, "Failed to process modinfo: %s\n", json_error_message());
		return MODINFO_CFG_INVALID_OPT;
	}
	if(bval) {
		modvar_set(modvar_create_dn(var_name), "1");
	}
	
	return MODINFO_OK;
}

int modinfo_read_wltypes(json_node_t* wltypes) {
	json_node_t* wltype;
	modvar_t* wltlist;
	
	int wltid = 0;
	
	int ret;
	
	if(wltypes == NULL || json_size(wltypes) == 0) {
		return MODINFO_OK;
	}
	
	wltlist = modvar_set_strlist(modvar_create("WLT_LIST"), json_size(wltypes));
	
	json_for_each(wltypes, wltype, wltid) {
		var_namespace = json_name(wltype);
		
		modvar_add_string(wltlist, wltid, json_name(wltype));
		
		ret = modinfo_parse_wlt_class(wltype);
		if(ret != MODINFO_OK)
			return ret;

		ret = modinfo_parse_wlparams(wltype);
		if(ret != MODINFO_OK)
			return ret;
		
		ret = modinfo_proc_bool(wltype, "has_step", "HAS_STEP");
		if(ret != MODINFO_OK)
			return ret;

		ret = modinfo_proc_bool(wltype, "has_data", "HAS_DATA");
		if(ret != MODINFO_OK)
			return ret;
		
		ret = modinfo_read_vars_wltype(json_name(wltype), wltype);
		if(ret != MODINFO_OK)
			return ret;
		
		var_namespace = NULL;
	}

	return MODINFO_OK;
}

void modinfo_destroy_config(void* obj) {
	json_node_t* root = (json_node_t*) obj;

	json_node_destroy(root);
}

int modinfo_read_config(const char* modinfo_path) {
	json_node_t* root;
	json_node_t* vars = NULL;
	json_node_t* wltypes = NULL;

	char* modname = NULL;
	char* modtype = "load";

	int ret = MODINFO_OK;

	json_buffer_t* buf = json_buf_from_file(modinfo_path);

	if(buf == NULL) {
		fprintf(stderr, "Failed to open modinfo: %s\n", json_error_message());
		return MODINFO_CFG_CANNOT_OPEN;
	}

	if(json_parse(buf, &root) != JSON_OK) {
		ret = MODINFO_CFG_PARSE_ERROR;
		goto bad_json;
	}

	modvar_set_gen(modvar_create("_CONFIG"), NULL,
				   modinfo_destroy_config, root);

	/* Read global module vars: MODNAME and MODTYPE */
	if(json_get_string(root, "name", &modname) != JSON_OK) {
		ret = MODINFO_CFG_MISSING_NAME;
		goto bad_json;
	}
	modvar_set(modvar_create("MODNAME"), modname);

	if(json_get_string(root, "type", &modtype) == JSON_INVALID_TYPE) {		
		ret = MODINFO_CFG_INVALID_OPT;
		goto bad_json;
	}
	modvar_set(modvar_create("MODTYPE"), modtype);
	
	/* Process workload types */
	if(json_get_node(root, "wltypes", &wltypes) == JSON_INVALID_TYPE) {		
		ret = MODINFO_CFG_INVALID_OPT;
		goto bad_json;
	}
	ret = modinfo_read_wltypes(wltypes);
	if(ret != MODINFO_OK)
		return ret;
		
	/* Redefine variables where requested */

	ret = modinfo_read_vars(modname, root);
	return ret;

bad_json:
	fprintf(stderr, "Failed to process modinfo: %s\n", json_error_message());
	return ret;
}

int modinfo_create_cflags(json_node_t* bldenv, const char* varname, const char* prefix) {
	json_node_t* cflags;
	json_node_t* cflag;
	int cfid;
	char* cflag_str;

	size_t len = 0, cflag_len, capacity = 512;
	char* str = NULL;

	int err;
	int ret = MODINFO_OK;
	const char* errmsg;

	if(json_get_array(bldenv, varname, &cflags) != JSON_OK) {
		ret = MODINFO_CFG_INVALID_OPT;
		errmsg = json_error_message();
		goto end;
	}

	/* Assemble CFLAGS string */
	str = mp_malloc(capacity);

	json_for_each(cflags, cflag, cfid) {
		cflag_str = json_as_string(cflag);
		if(cflag_str == NULL) {
			ret = MODINFO_CFG_INVALID_OPT;
			errmsg = json_error_message();
			goto end;
		}

		cflag_len = strlen(cflag_str);
		while(cflag_len > (capacity - len)) {
			capacity += 512;
			str = mp_realloc(str, capacity);
		}

		if(strchr(cflag_str, ' ') == NULL)
			err = snprintf(str + len, capacity - len, "%s%s ", prefix, cflag_str);
		else
			err = snprintf(str + len, capacity - len, "\"%s%s\" ", prefix, cflag_str);

		if(err < 0) {
			ret = MODINFO_CFG_INVALID_OPT;
			errmsg = "snprintf returned -1";
			goto end;
		}

		len += err;
	}

	/* Everything went fine, define a variable */
	modvar_set(modvar_create_dn(varname), str);

end:
	if(str)
		mp_free(str);
	if(ret != MODINFO_OK)
		fprintf(stderr, "Failed to process build environment: %s\n", errmsg);
	return ret;
}

/**
 * Read build environment file buildenv.json that is generated during SCons build
 *
 * It is used to save some SCons variables set by main SConstruct and should be
 * reused by SConstructs of modules. By default, it is read by SCons internally,
 * but for GNU Make build we should read them from tsgenmodsrc and write to
 * Makefile explicitly.
 */
int modinfo_read_buildenv(const char* root_path, const char* bldenv_path) {
	json_node_t* bldenv;
	int ret = MODINFO_OK;
	char path[PATHMAXLEN];
	char* value;

	json_buffer_t* buf = NULL;

	path_join(path, PATHMAXLEN, root_path, bldenv_path, NULL);
	buf = json_buf_from_file(path);

	if(buf == NULL) {
		fprintf(stderr, "Failed to open build environment: %s\n", json_error_message());
		return MODINFO_CFG_CANNOT_OPEN;
	}

	if(json_parse(buf, &bldenv) != JSON_OK) {
		fprintf(stderr, "Failed to parse build environment: %s\n", json_error_message());
		return MODINFO_CFG_PARSE_ERROR;
	}

	ret = modinfo_create_cflags(bldenv, "CCFLAGS", "");
	if(ret != MODINFO_OK)
		goto end;

	ret = modinfo_create_cflags(bldenv, "SHCCFLAGS", "");
	if(ret != MODINFO_OK)
		goto end;

	ret = modinfo_create_cflags(bldenv, "LIBS", "-l");
	if(ret != MODINFO_OK)
		goto end;

	ret = modinfo_create_cflags(bldenv, "SHLINKFLAGS", "");
	if(ret != MODINFO_OK)
		goto end;

	ret = modinfo_create_cflags(bldenv, "LINKFLAGS", "");
	if(ret != MODINFO_OK)
		goto end;

	if(json_get_string(bldenv, "SHOBJSUFFIX", &value) != JSON_OK) {
		ret = MODINFO_CFG_INVALID_OPT;
		goto end;
	}
	modvar_set(modvar_create("SHOBJSUFFIX"), value);

	if(json_get_string(bldenv, "SHLIBSUFFIX", &value) != JSON_OK) {
		ret = MODINFO_CFG_INVALID_OPT;
		goto end;
	}
	modvar_set(modvar_create("SHLIBSUFFIX"), value);

end:
	json_node_destroy(bldenv);

	return ret;
}
