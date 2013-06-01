/*
 * swatmap.c
 *
 *  Created on: 18.01.2013
 *      Author: myaut
 */

#include <defs.h>
#include <swatmap.h>

list_head_t swat_dev_map;

/** SWAT mappings
 *
 * Maps swat device ids (for Solaris i.e. dev_t values) to paths on target system
 * Uses linked list because it is simpler than hashmap  */

/**
 * Add mapping from dev to path */
int swat_add_mapping(uint64_t dev, const char* path) {
	swat_map_item_t *item;

	if(swat_get_mapping(dev) != NULL)
		return -1;

	item = (swat_map_item_t*) malloc(sizeof(swat_map_item_t));

	item->dev = dev;
	strncpy(item->path, path, SWAPMAPPATHLEN);

	list_add(&item->node, &swat_dev_map);

	return 0;
}

/**
 * Get mapping for dev*/
const char* swat_get_mapping(uint64_t dev) {
	swat_map_item_t *item;

	list_for_each_entry(swat_map_item_t, item, &swat_dev_map, node) {
		if(item->dev == dev)
			return item->path;
	}

	return NULL;
}

int swat_map_init(void) {
	list_head_init(&swat_dev_map, "swat_dev_map");
}

void swat_map_fini(void) {
	swat_map_item_t *item, *temp;

	list_for_each_entry_safe(swat_map_item_t, item, temp, &swat_dev_map, node) {
		free(item);
	}
}
