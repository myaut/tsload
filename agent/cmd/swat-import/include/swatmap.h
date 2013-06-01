/*
 * swatmap.h
 *
 *  Created on: 18.01.2013
 *      Author: myaut
 */

#ifndef SWATMAP_H_
#define SWATMAP_H_

#include <stdint.h>
#include <list.h>

#define SWAPMAPPATHLEN	512

typedef struct {
	uint64_t dev;
	const char path[SWAPMAPPATHLEN];

	list_node_t	node;
} swat_map_item_t;

int swat_add_mapping(uint64_t dev, const char* path);
const char* swat_get_mapping(uint64_t dev);

int swat_map_init(void);
void swat_map_fini(void);

#endif /* SWATMAP_H_ */
