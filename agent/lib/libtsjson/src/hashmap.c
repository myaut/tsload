/*
 * hashmap.c
 *
 *  Created on: Aug 31, 2014
 *      Author: myaut
 */

#include <hashmap.h>

#include <json.h>

STATIC_INLINE hm_key_t* hm_get_key(hashmap_t* hm, hm_item_t* obj) {
	return (obj + hm->hm_off_key);
}

int json_hm_walker(hm_item_t* object, void* arg) {
	struct hm_fmt_context* context = (struct hm_fmt_context*) arg;
	json_node_t* item_node = context->formatter(object);

	json_str_t name;

	if(context->key_formatter == NULL) {
		name = json_str_create(hm_get_key(context->hm, object));
	}
	else {
		name = context->key_formatter(name);
	}

	if(item_node != NULL)
		json_add_node(context->node, name, item_node);

	return HM_WALKER_CONTINUE;
}


json_node_t* json_hm_format_bykey(hashmap_t* hm, hm_json_formatter formatter, void* key) {
	hm_item_t* object = hash_map_find(hm, key);

	if(!object)
		return NULL;

	return formatter(object);
}

json_node_t* json_hm_format_all(hashmap_t* hm, hm_json_formatter formatter,
		hm_json_key_formatter key_formatter) {
	json_node_t* node = json_new_node(NULL);
	struct hm_fmt_context context;

	context.hm = hm;
	context.node = node;
	context.formatter = formatter;
	context.key_formatter = key_formatter;

	hash_map_walk(hm, json_hm_walker, &context);

	return node;
}
