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

#ifndef GENMODSRC_H_
#define GENMODSRC_H_

#include <tsload/defs.h>
#include <tsload/autostring.h>

#include <stdio.h>
#include <stdarg.h>

#define VARHASHSIZE				32
#define VARHASHMASK				(VARHASHSIZE - 1)

#define MODTARGETPARTS			8

#define DEVEL_DIR				"devel"
#define BUILD_DIR				"build/build"
#define MODSRC_DIR				"modsrc"
#define HEADER_IN_FN			"modsrc.h.in"
#define SOURCE_IN_FN			"modsrc.c.in"
#define BUILDENV_FN				"make.bldenv.json"
#define MAKEFILE_IN_FN			"Makefile.in"
#define SCONSCRIPT_IN_FN		"SConscript.in"
#define SCONSTRUCT_IN_FN		"SConstruct.in"
#define CTIME_CACHE_FN			".ctime_cache"

#define MOD_BUILD_SCONS			0
#define MOD_BUILD_MAKE			1

#define MOD_INTERNAL			0
#define MOD_EXTERNAL			1

#define MODSRC_SHOW_VARS			0
#define MODSRC_SHOW_HEADER			1
#define MODSRC_SHOW_SOURCE			2
#define MODSRC_SHOW_MAKEFILE		3
#define MODSRC_GENERATE				4
#define MODSRC_CLEAN				5

#define MODVAR_OK				0
#define MODVAR_NOTFOUND			-1

#define MODINFO_OK					0
#define MODINFO_DIR_ERROR			-1
#define MODINFO_DIR_NOT_EMPTY		-2
#define MODINFO_CFG_CANNOT_OPEN		-3
#define MODINFO_CFG_PARSE_ERROR		-4
#define MODINFO_CFG_MISSING_NAME	-5
#define MODINFO_CFG_INVALID_OPT		-6
#define MODINFO_CFG_INVALID_VAR		-7
#define MODINFO_CFG_NO_WLT_CLASS	-8
#define MODINFO_CFG_INVAL_WLT_CLASS	-9
#define MODINFO_CFG_NO_PARAMS		-10
#define MODINFO_CFG_INVALID_PARAM	-11

typedef void (*modsrc_value_gen_func)(FILE* outf, void* arg);
typedef void (*modsrc_dtor_func)(void* arg);

typedef struct modvar {
	AUTOSTRING char* name;

	AUTOSTRING char* value;

	modsrc_value_gen_func valgen;
	modsrc_dtor_func valgen_dtor;
	void* valgen_arg;

	struct modvar* 	next;
} modvar_t;

typedef struct modtarget {
	const char* src;
	const char* dst[MODTARGETPARTS];
} modtarget_t;

#define MOD_TARGET(src, ...) 		{src, { __VA_ARGS__, NULL } }
#define MOD_TARGET_ROOT(src, ...)	{src, { "@MODINFODIR", __VA_ARGS__, NULL } }
#define MOD_TARGET_BUILD(src, ...)	{src, { "@MODINFODIR", BUILD_DIR, "@MODNAME", "@MODNAME", __VA_ARGS__, NULL } }
#define MOD_TARGET_MOD(src, ...)	{src, { "@MODINFODIR", "@MODNAME", __VA_ARGS__, NULL } }
#define MOD_TARGET_END				MOD_TARGET(NULL, NULL)

typedef int (*modsrc_generate_walk_func)(modtarget_t* tgt, const char* path, const char* dirname, void* arg);

modvar_t* modvar_create_impl(int nametype, const char* name);
#define modvar_create(name)													\
		modvar_create_impl(AAS_STATIC, AAS_STATIC_PREFIX name)
#define modvar_create_dn(name)												\
		modvar_create_impl(AAS_DYNAMIC, name)

STATIC_INLINE modvar_t* modvar_set(modvar_t* var, const char* value) {
	if(var == NULL)
		return NULL;

	aas_copy(&var->value, value);
	return var;
}
STATIC_INLINE modvar_t* modvar_vprintf(modvar_t* var, const char* fmtstr, va_list va) {
	if(var == NULL)
		return NULL;

	aas_vprintf(&var->value, fmtstr, va);

	return var;
}

modvar_t* modvar_printf(modvar_t* var, const char* fmtstr, ...);

modvar_t* modvar_set_gen(modvar_t* var, modsrc_value_gen_func valgen,
					 	 modsrc_dtor_func dtor, void* arg);

modvar_t* modvar_get(const char* name);
int modvar_unset(const char* name);
boolean_t modvar_is_set(const char* name);

void modvar_destroy(modvar_t* var);

void modsrc_dump_vars(void);

int modinfo_check_dir(const char* modinfo_path, boolean_t silent);
int modinfo_read_config(const char* modinfo_path);

int modinfo_read_buildenv(const char* root_path, const char* bldenv_path);

int modsrc_init(void);
void modsrc_fini(void);

#endif /* GENMODSRC_H_ */
