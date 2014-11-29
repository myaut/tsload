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

#include <tsload/defs.h>

#include <tsload/hashmap.h>

#include <tsload/obj/obj.h>


STATIC_INLINE hm_key_t* hm_get_key(hashmap_t* hm, hm_item_t* obj) {
	return (obj + hm->hm_off_key);
}

int tsobj_hm_walker(hm_item_t* object, void* arg) {
	struct hm_fmt_context* context = (struct hm_fmt_context*) arg;
	tsobj_node_t* item_node = context->formatter(object);

	tsobj_str_t name;

	if(context->key_formatter == NULL) {
		name = tsobj_str_create(hm_get_key(context->hm, object));
	}
	else {
		name = context->key_formatter(name);
	}

	if(item_node != NULL)
		tsobj_add_node(context->node, name, item_node);

	return HM_WALKER_CONTINUE;
}


tsobj_node_t* tsobj_hm_format_bykey(hashmap_t* hm, hm_tsobj_formatter formatter, void* key) {
	hm_item_t* object = hash_map_find(hm, key);

	if(!object)
		return NULL;

	return formatter(object);
}

tsobj_node_t* tsobj_hm_format_all(hashmap_t* hm, hm_tsobj_formatter formatter,
		hm_tsobj_key_formatter key_formatter) {
	tsobj_node_t* node = tsobj_new_node(NULL);
	struct hm_fmt_context context;

	context.hm = hm;
	context.node = node;
	context.formatter = formatter;
	context.key_formatter = key_formatter;

	hash_map_walk(hm, tsobj_hm_walker, &context);

	return node;
}
