#define NO_JSON
#define JSONNODE void

#define LOG_SOURCE "modules"
#include <log.h>

#include <mempool.h>
#include <defs.h>
#include <modules.h>
#include <wltype.h>
#include <wlparam.h>
#include <hashmap.h>

#include <tsobj.h>

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
	tsobj_str_t name;
} wlt_class_names[] = {
	WL_CLASS_NAME(WLC_CPU_INTEGER , TSOBJ_STR("cpu_integer")),
	WL_CLASS_NAME(WLC_CPU_FLOAT, TSOBJ_STR("cpu_float")),
	WL_CLASS_NAME(WLC_CPU_MEMORY, TSOBJ_STR("cpu_memory")),
	WL_CLASS_NAME(WLC_CPU_MISC, TSOBJ_STR("cpu_misc")),

	WL_CLASS_NAME(WLC_MEMORY_ALLOCATION , TSOBJ_STR("mem_allocation")),

	WL_CLASS_NAME(WLC_FILESYSTEM_OP, TSOBJ_STR("fs_op")),
	WL_CLASS_NAME(WLC_FILESYSTEM_RW, TSOBJ_STR("fs_rw")),

	WL_CLASS_NAME(WLC_DISK_RW, TSOBJ_STR("disk_rw")),

	WL_CLASS_NAME(WLC_NETWORK, TSOBJ_STR("network")),

	WL_CLASS_NAME(WLC_OS_BENCHMARK, TSOBJ_STR("os"))
};

#define WL_CLASS_NUM		10

/**
 * Register TSLoad workload type (called in mod_config)
 *
 * @param mod module descriptor
 * @param wlt workload type descriptor
 */
int wl_type_register(module_t* mod, wl_type_t* wlt) {
	wlt->wlt_module = mod;
	wlt->wlt_next = NULL;

	return hash_map_insert(&wl_type_hash_map, wlt);
}

/**
 * Unregister TSLoad workload type (called in mod_unconfig)
 *
 * @param mod module descriptor
 * @param wlt workload type descriptor
 */
int wl_type_unregister(module_t* mod, wl_type_t* wlt) {
	return hash_map_remove(&wl_type_hash_map, wlt);
}

wl_type_t* wl_type_search(const char* name) {
	void* object = hash_map_find(&wl_type_hash_map, name);
	wl_type_t* wlt = (wl_type_t*) object;

	return wlt;
}

struct tsobj_wl_type_context {
	tsobj_node_t* node;
};

tsobj_node_t* tsobj_wl_type_format(hm_item_t* object) {
	wl_type_t* wlt = (wl_type_t*) object;
	int i;

	tsobj_node_t* wlt_node = json_new_node("tsload.WorkloadType");
	tsobj_node_t* wlt_class = json_new_array();

	tsobj_add_string(wlt_node, TSOBJ_STR("module"), tsobj_str_create(wlt->wlt_module->mod_name));
	tsobj_add_string(wlt_node, TSOBJ_STR("path"), tsobj_str_create(wlt->wlt_module->mod_path));

	for(i = 0; i < WL_CLASS_NUM; ++i) {
		if(wlt->wlt_class & wlt_class_names[i].wlc) {
			tsobj_add_string(wlt_class, NULL, wlt_class_names[i].name);
		}
	}

	tsobj_add_node(wlt_node, TSOBJ_STR("wlclass"), wlt_class);
	tsobj_add_node(wlt_node, TSOBJ_STR("parameters"), tsobj_wlparam_format_all(wlt->wlt_params));

	return wlt_node;
}

int wlt_init(void) {
	hash_map_init(&wl_type_hash_map, "wl_type_hash_map");

	return 0;
}

void wlt_fini(void) {
	hash_map_destroy(&wl_type_hash_map);
}
