/*
 * hashmap.c
 *
 *  Created on: Aug 31, 2014
 *      Author: myaut
 */

#include <hashmap.h>

#include <json.h>

int json_hm_walker(hm_item_t* object, void* arg) {
	struct hm_fmt_context* context = (struct hm_fmt_context*) arg;
	json_node_t* item_node = context->formatter(object);

	if(item_node != NULL)
		json_push_back(context->node, item_node);

	return HM_WALKER_CONTINUE;
}


json_node_t* json_hm_format_bykey(hashmap_t* hm, hm_json_formatter formatter, void* key) {
	hm_item_t* object = hash_map_find(hm, key);

	if(!object)
		return NULL;

	return formatter(object);
}

json_node_t* json_hm_format_all(hashmap_t* hm, hm_json_formatter formatter) {
	json_node_t* node = json_new_node(NULL);
	struct hm_fmt_context context;

	context.node = node;
	context.formatter = formatter;

	hash_map_walk(hm, json_hm_walker, &context);

	return node;
}
