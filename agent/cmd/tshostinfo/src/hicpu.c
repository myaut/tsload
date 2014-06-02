
/*
    This file is part of TSLoad.
    Copyright 2013-2014, Sergey Klyaus, ITMO University

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



#include <defs.h>
#include <cpuinfo.h>
#include <hiprint.h>

#include <string.h>

static
int cache_type_char(hi_cpu_cache_type_t type) {
	switch(type) {
	case HI_CPU_CACHE_DATA:
		return 'd';
	case HI_CPU_CACHE_INSTRUCTION:
		return 'i';
	case HI_CPU_CACHE_UNIFIED:
		return ' ';
	}

	return '?';
}

void print_cpu_element(hi_object_t* parent, int indent) {
	hi_object_child_t* child;
	hi_cpu_object_t* object;
	char indent_str[16];
	int ci;
	long page_size;

	indent = min(indent, 15);
	memset(indent_str, ' ', indent);
	indent_str[indent] = '\0';

	hi_for_each_child(child, parent) {
		object = HI_CPU_FROM_OBJ(child->object);

		switch(object->type) {
		case HI_CPU_NODE:
			/* Inception */
			break;
		case HI_CPU_CHIP:
			printf("%s+----%-s %lu MHz %s\n", indent_str, object->hdr.name,
										(unsigned long) object->chip.cp_freq,
										object->chip.cp_name);
			break;
		case HI_CPU_CORE:
		case HI_CPU_STRAND:
			printf("%s+----%-s\n", indent_str, object->hdr.name);
			break;
		case HI_CPU_CACHE:
			if(object->cache.c_level != HI_CPU_CACHE_TLB_LEVEL) {
				printf("%s+----%-s L%d%c %d %d:%d\n", indent_str, object->hdr.name, object->cache.c_level,
						cache_type_char(object->cache.c_type), object->cache.c_size,
						object->cache.c_unit_size.line, object->cache.c_associativity);
			}
			else {
				printf("%s+----%-s %cTLB %d %d sizes: ", indent_str, object->hdr.name,
										cache_type_char(object->cache.c_type), object->cache.c_size,
										object->cache.c_associativity);
				for(ci = 0; ci < 4; ++ci) {
					page_size = object->cache.c_unit_size.page[ci];

					if(page_size == 0)
						break;

					printf(" %ld", page_size);
				}

				fputs("\n", stdout);
			}
			break;
		}
		print_cpu_element(child->object, indent + 5);
	}
}

int print_cpu_info(int flags) {
	list_head_t* cpu_list = hi_cpu_list(B_FALSE);
	hi_object_t*     object;
	hi_cpu_object_t* node;

	print_header(flags, "CPU TOPOLOGY");

	if(flags & INFO_LEGEND) {
		printf("%-4s %-4s %-4s %-4s %-12s %-12s\n", "NODE", "CHIP", "CORE",
				"STR", "MEM TOTAL", "MEM FREE");
	}

	hi_for_each_object(object, cpu_list) {
		node = HI_CPU_FROM_OBJ(object);

		if(node->type == HI_CPU_NODE) {
			printf("%-4d %-4s %-4s %-4s %-12lld %-12lld\n", node->id, "", "",
							"", node->node.cm_mem_total, node->node.cm_mem_free);

			if(flags & INFO_XDATA) {
				print_cpu_element(object, 0);
			}
		}
		else if(!(flags & INFO_XDATA) &&
				node->type == HI_CPU_CHIP) {
					printf("\t%-3d %lld %s\n", node->id, node->chip.cp_freq,
									node->chip.cp_name);
		}

	}

	return INFO_OK;
}

