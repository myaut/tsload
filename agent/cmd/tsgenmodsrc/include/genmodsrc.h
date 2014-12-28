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

#define DEVEL_DIR				"devel"

#define MOD_BUILD_SCONS			0
#define MOD_BUILD_MAKE			1

#define MOD_INTERNAL			0
#define MOD_EXTERNAL			1

#define MODVAR_OK				0
#define MODVAR_NOTFOUND			-1

#define MODINFO_OK					0
#define MODINFO_DIR_ERROR			-1
#define MODINFO_DIR_NOT_EMPTY		-2
#define MODINFO_CFG_CANNOT_OPEN		-3
#define MODINFO_CFG_PARSE_ERROR		-4
#define MODINFO_CFG_MISSING_NAME	-5
#define MODINFO_CFG_INVALID_VAR		-6

typedef int (*modsrc_value_gen_func)(FILE* f);

typedef struct modvar {
	AUTOSTRING char* name;

	AUTOSTRING char* value;
	modsrc_value_gen_func value_generator;

	struct modvar* 	next;
} modvar_t;

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
modvar_t* modvar_gen(modvar_t* var, modsrc_value_gen_func value_gen);
int modvar_unset(const char* name);

void modvar_destroy(modvar_t* var);

void modsrc_dump_vars(void);

int modinfo_check_dir(const char* modinfo_path);
int modinfo_read_config(const char* modinfo_path);

int modsrc_init(void);
void modsrc_fini(void);

#endif /* GENMODSRC_H_ */
