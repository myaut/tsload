
/*
    This file is part of TSLoad.
    Copyright 2013, Sergey Klyaus, ITMO University

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

