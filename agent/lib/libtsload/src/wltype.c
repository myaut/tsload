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

#define WL_CLASS_NAME(wlc_, name_) \
		{ SM_INIT(.wlc, wlc_), SM_INIT(.name, name_) }

struct wlt_class_name {
	unsigned long wlc;
	const char* name;
} wlt_class_names[] = {
	WL_CLASS_NAME(WLC_CPU_INTEGER , "cpu_integer"),
	WL_CLASS_NAME(WLC_CPU_FLOAT, "cpu_float"),
	WL_CLASS_NAME(WLC_CPU_MEMORY, "cpu_memory"),
	WL_CLASS_NAME(WLC_CPU_MISC, "cpu_misc"),

	WL_CLASS_NAME(WLC_MEMORY_ALLOCATION , "mem_allocation"),

	WL_CLASS_NAME(WLC_FILESYSTEM_OP, "fs_op"),
	WL_CLASS_NAME(WLC_FILESYSTEM_RW, "fs_rw"),

	WL_CLASS_NAME(WLC_DISK_RW, "disk_rw"),

	WL_CLASS_NAME(WLC_NETWORK, "network"),

	WL_CLASS_NAME(WLC_OS_BENCHMARK, "os")
};

#define WL_CLASS_NUM		10

int wl_type_register(module_t* mod, wl_type_t* wlt) {
	wlt->wlt_module = mod;
	wlt->wlt_next = NULL;

	return hash_map_insert(&wl_type_hash_map, wlt);
}

int wl_type_unregister(module_t* mod, wl_type_t* wlt) {
	return hash_map_remove(&wl_type_hash_map, wlt);
}

wl_type_t* wl_type_search(const char* name) {
	void* object = hash_map_find(&wl_type_hash_map, name);
	wl_type_t* wlt = (wl_type_t*) object;

	return wlt;
}

struct json_wl_type_context {
	JSONNODE* node;
};

JSONNODE* json_wl_type_format(hm_item_t* object) {
	wl_type_t* wlt = (wl_type_t*) object;
	int i;
	JSONNODE* wlt_node = json_new(JSON_NODE);
	JSONNODE* wlt_class = json_new(JSON_ARRAY);

	json_set_name(wlt_node, wlt->wlt_name);
	json_set_name(wlt_class, "wlclass");

	json_push_back(wlt_node, json_new_a("module", wlt->wlt_module->mod_name));
	json_push_back(wlt_node, json_new_a("path", wlt->wlt_module->mod_path));
	json_push_back(wlt_node, wlt_class);

	for(i = 0; i < WL_CLASS_NUM; ++i) {
		if(wlt->wlt_class & wlt_class_names[i].wlc) {
			json_push_back(wlt_class, json_new_a("", wlt_class_names[i].name));
		}
	}

	json_push_back(wlt_node, json_wlparam_format_all(wlt->wlt_params));

	return wlt_node;
}

int wlt_init(void) {
	hash_map_init(&wl_type_hash_map, "wl_type_hash_map");

	return 0;
}

void wlt_fini(void) {
	hash_map_destroy(&wl_type_hash_map);
}
