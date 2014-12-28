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

#include <tsload/mempool.h>
#include <tsload/hashmap.h>

#include <genmodsrc.h>

#include <stdio.h>

mp_cache_t var_cache;

DECLARE_HASH_MAP_STRKEY(var_hash_map, modvar_t, VARHASHSIZE, name, next, VARHASHMASK);

modvar_t* modvar_create_impl(int nametype, const char* name) {
	modvar_t* var = mp_cache_alloc(&var_cache);

	if(var == NULL)
		return NULL;

	if(likely(nametype == AAS_STATIC))
		aas_set_impl(&var->name, name);
	else
		aas_copy(aas_init(&var->name), name);

	aas_init(&var->value);
	var->value_generator = NULL;

	var->next = NULL;

	if(hash_map_insert(&var_hash_map, var) != HASH_MAP_OK) {
		modvar_destroy(var);
		return NULL;
	}

	return var;
}

void modvar_destroy(modvar_t* var) {
	aas_free(&var->value);
	aas_free(&var->name);
	mp_cache_free(&var_cache, var);
}

modvar_t* modvar_printf(modvar_t* var, const char* fmtstr, ...) {
	va_list va;

	if(var == NULL)
		return NULL;

	va_start(va, fmtstr);
	aas_vprintf(&var->value, fmtstr, va);
	va_end(va);

	return var;
}

modvar_t* modvar_gen(modvar_t* var, modsrc_value_gen_func value_gen) {
	if(var == NULL)
		return NULL;

	var->value_generator = value_gen;

	return var;
}

int modvar_unset(const char* name) {
	modvar_t* var = hash_map_find(&var_cache, name);

	if(var == NULL)
		return MODVAR_NOTFOUND;

	hash_map_remove(&var_cache, var);
	modvar_destroy(var);

	return MODVAR_OK;
}


/* modsrc_dump_vars
 * ----------------------- */
static int modvar_dump(hm_item_t* object, void* arg) {
	modvar_t* var = (modvar_t*) object;

	fprintf(stdout, "%s=%s\n", var->name,
				(var->value)
					? var->value
					: "generated");

	return HM_WALKER_CONTINUE;
}

void modsrc_dump_vars(void) {
	hash_map_walk(&var_hash_map, modvar_dump, NULL);
}

int modsrc_init(void) {
	mp_cache_init(&var_cache, modvar_t);
	hash_map_init(&var_hash_map, "var_hash_map");

	return 0;
}

static int modvar_destroy_walker(hm_item_t* object, void* arg) {
	modvar_destroy((modvar_t*) object);

	return HM_WALKER_REMOVE | HM_WALKER_CONTINUE;
}

void modsrc_fini(void) {
	hash_map_walk(&var_hash_map, modvar_destroy_walker, NULL);
	hash_map_destroy(&var_hash_map);

	mp_cache_destroy(&var_cache);
}
