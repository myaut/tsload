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

const char* var_namespace = NULL;
mp_cache_t var_cache;

DECLARE_HASH_MAP(var_hash_map, B_FALSE, modvar_t, VARHASHSIZE, v_name, v_next, 
	{
		modvar_name_t* vn = (modvar_name_t*) key;
		unsigned ns_hash = 0;
		if(vn->ns != NULL) 
			ns_hash = hm_string_hash(vn->ns, VARHASHMASK);
		return ns_hash ^ hm_string_hash(vn->name, VARHASHMASK);
	},
	{
		modvar_name_t* vn1 = (modvar_name_t*) key1;
		modvar_name_t* vn2 = (modvar_name_t*) key2;
		
		if((vn1->ns != NULL && vn2->ns == NULL) ||
		   (vn1->ns == NULL && vn2->ns != NULL))
			return B_FALSE;
		
		if(vn1->ns != NULL && vn2->ns != NULL && 
				strcmp(vn1->ns, vn2->ns) != 0)
			return B_FALSE;
		
		return strcmp(vn1->name, vn2->name) == 0;
	}
)

modvar_t* modvar_create_impl(int nametype, const char* name) {
	modvar_t* var = mp_cache_alloc(&var_cache);

	if(var == NULL)
		return NULL;

	aas_init(&var->v_name.ns);
	if(var_namespace) {
		aas_copy(&var->v_name.ns, var_namespace);
	}
	
	/* To save allocations, modvar_create() passes name directly to var->name and sets nametype.*/
	if(likely(nametype == AAS_STATIC))
		aas_set_impl(&var->v_name.name, name);
	else
		aas_copy(aas_init(&var->v_name.name), name);

	var->v_type = VAR_UNKNOWN;
	var->v_next = NULL;

	if(hash_map_insert(&var_hash_map, var) != HASH_MAP_OK) {
		fprintf(stderr, "WARNING: variable '%s' already exists in namespace '%s'\n", 
				var->v_name.name, (var_namespace == NULL) ? "(global)" : var_namespace);
		
		modvar_destroy(var);
		return NULL;
	}

	return var;
}

void modvar_destroy(modvar_t* var) {
	switch(var->v_type) {
		case VAR_STRING:
			aas_free(&var->v_value.str);
		break;
		case VAR_GENERATOR:
		{
			mod_valgen_t* vg = &var->v_value.gen;
			if(vg->dtor)
				vg->dtor(vg->arg);
		}
		break;
		case VAR_STRING_LIST: {
			int i;
			mod_strlist_t* sl = &var->v_value.sl;
			for(i = 0; i < sl->sl_count; ++i) {
				aas_free(&sl->sl_list[i]);
			}
			
			mp_free(sl->sl_list);
		}
		break;
	}
	
	aas_free(&var->v_name.ns);
	aas_free(&var->v_name.name);
		
	mp_cache_free(&var_cache, var);
}

modvar_t* modvar_printf(modvar_t* var, const char* fmtstr, ...) {
	va_list va;

	if(var == NULL)
		return NULL;

	va_start(va, fmtstr);
	aas_vprintf(&var->v_value.str, fmtstr, va);
	va_end(va);
	
	var->v_type = VAR_STRING;

	return var;
}

modvar_t* modvar_set_gen(modvar_t* var, modsrc_value_gen_func valgen,
					 	 modsrc_dtor_func dtor, void* arg) {
	mod_valgen_t* vg;
	
	if(var == NULL)
		return NULL;
	
	vg = &var->v_value.gen;
	vg->valgen = valgen;
	vg->dtor = dtor;
	vg->arg = arg;
	
	var->v_type = VAR_GENERATOR;

	return var;
}

modvar_t* modvar_set_strlist(modvar_t* var, int count) {
	mod_strlist_t* sl;
	int i;
	
	if(var == NULL)
		return NULL;
	
	sl = &var->v_value.sl;
	sl->sl_count = count;
	sl->sl_list = mp_malloc(count * sizeof(char*));
	
	for(i = 0; i < count; ++i) {
		aas_init(&sl->sl_list[i]);
	}
	
	var->v_type = VAR_STRING_LIST;
	
	return var;
}

modvar_t* modvar_add_string(modvar_t* var, int i, const char* str) {
	mod_strlist_t* sl;
	
	if(var == NULL || var->v_type != VAR_STRING_LIST)
		return NULL;
	
	sl = &var->v_value.sl;
	if(i > sl->sl_count)
		return NULL;
	
	aas_copy(&sl->sl_list[i], str);
	
	return var;
}

modvar_t* modvar_get(const char* name) {
	modvar_name_t vn;
	modvar_t* var;
	
	/* Variables kept in var hash table should use autostrings
	   but in our case we only need search criteria, so hack
	   our way by using direct values of constant strings. */
	vn.ns = (char*) var_namespace;
	vn.name = (char*) name;

	/* Search in local namespace */
	var = hash_map_find(&var_hash_map, &vn);
	if(var != NULL)
		return var;
	
	/* Try global namespace */
	vn.ns = NULL;
	return hash_map_find(&var_hash_map, &vn);
}

const char* modvar_get_string(const char* name) {
	modvar_t* var = modvar_get(name);
	
	if(var == NULL || var->v_type != VAR_STRING)
		return NULL;
	
	return var->v_value.str;
}

void* modvar_get_arg(const char* name) {
	modvar_t* var = modvar_get(name);
	
	if(var == NULL || var->v_type != VAR_GENERATOR)
		return NULL;
	
	return var->v_value.gen.arg;
}

mod_valgen_t* modvar_get_valgen(const char* name) {
	modvar_t* var = modvar_get(name);
	
	if(var == NULL || var->v_type != VAR_GENERATOR)
		return NULL;
	
	return &var->v_value.gen;
}

mod_strlist_t* modvar_get_strlist(const char* name) {
	modvar_t* var = modvar_get(name);
	
	if(var == NULL || var->v_type != VAR_STRING_LIST)
		return NULL;
	
	return &var->v_value.sl;
}

int modvar_unset(const char* name) {
	modvar_t* var = modvar_get(name);

	if(var == NULL)
		return MODVAR_NOTFOUND;

	hash_map_remove(&var_hash_map, var);
	modvar_destroy(var);

	return MODVAR_OK;
}

boolean_t modvar_is_set(const char* name) {
	modvar_t* var = modvar_get(name);

	if(var == NULL)
		return B_TRUE;

	return B_FALSE;
}


/* modsrc_dump_vars
 * ----------------------- */
static int modvar_dump(hm_item_t* object, void* arg) {
	modvar_t* var = (modvar_t*) object;

	if(var->v_name.ns != NULL)
		fprintf(stdout, "%s:", var->v_name.ns);
	fputs(var->v_name.name, stdout);
	
	switch(var->v_type) {
		case VAR_UNKNOWN:
			fputs(" [unknown]\n", stdout);
			break;
		case VAR_STRING:
			fprintf(stdout, "=%s\n", var->v_value.str);
			break;
		case VAR_STRING_LIST:
			fputs(" [stringlist]\n", stdout);
			break;
		case VAR_GENERATOR:
			fputs(" [generator]\n", stdout);
			break;
		default:
			fputs(" [???]\n", stdout);
			break;
	}

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
