#define LOG_SOURCE "modules"
#include <log.h>

#include <mempool.h>
#include <defs.h>
#include <modules.h>
#include <wltype.h>
#include <wlparam.h>
#include <hashmap.h>

#include <libjson.h>

#include <stdlib.h>

DECLARE_HASH_MAP(wl_type_hash_map, wl_type_t, WLTHASHSIZE, wlt_name, wlt_next,
	{
		return hm_string_hash(key, WLTHASHMASK);
	},
	{
		return strcmp((char*) key1, (char*) key2) == 0;
	}
)

int wl_type_register(module_t* mod, wl_type_t* wlt) {
	wlt->wlt_module = mod;
	wlt->wlt_next = NULL;

	return hash_map_insert(&wl_type_hash_map, wlt);
}

wl_type_t* wl_type_search(const char* name) {
	void* object = hash_map_find(&wl_type_hash_map, name);
	wl_type_t* wlt = (wl_type_t*) object;

	return wlt;
}

struct json_wl_type_context {
	JSONNODE* node;
};

static JSONNODE* json_wl_type_format_impl(hm_item_t* object) {
	wl_type_t* wlt = (wl_type_t*) object;
	JSONNODE* wlt_node = json_new(JSON_NODE);

	json_set_name(wlt_node, wlt->wlt_name);

	json_push_back(wlt_node, json_new_a("module", wlt->wlt_module->mod_name));
	json_push_back(wlt_node, json_new_a("path", wlt->wlt_module->mod_path));
	json_push_back(wlt_node, json_wlparam_format_all(wlt->wlt_params));

	return wlt_node;
}

JSONNODE* json_wl_type_format(const char* name) {
	return json_hm_format_bykey(&wl_type_hash_map, json_wl_type_format_impl, (void*) name);
}

JSONNODE* json_wl_type_format_all(void) {
	return json_hm_format_all(&wl_type_hash_map, json_wl_type_format_impl);
}


int wlt_init(void) {
	hash_map_init(&wl_type_hash_map, "wl_type_hash_map");

	return 0;
}

void wlt_fini(void) {
	hash_map_destroy(&wl_type_hash_map);
}
