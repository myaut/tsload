/*
 * hicpu.c
 *
 *  Created on: 02.05.2013
 *      Author: myaut
 */

#include <defs.h>
#include <cpuinfo.h>
#include <hiprint.h>

#include <string.h>

static
int cache_type_char(hi_cpu_cache_type_t type) {
	switch(type) {
	case HI_CPU_CACHE_DATA:
		return 'D';
	case HI_CPU_CACHE_INSTRUCTION:
		return 'I';
	case HI_CPU_CACHE_UNIFIED:
		return 'U';
	}

	return '?';
}

void print_cpu_element(hi_object_t* parent, int indent) {
	hi_object_child_t* child;
	hi_cpu_object_t* object;
	char indent_str[16];

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
			printf("%s+----%-s L%d%c %d %d:%d\n", indent_str, object->hdr.name, object->cache.c_level,
					cache_type_char(object->cache.c_type), object->cache.c_size,
					object->cache.c_line_size, object->cache.c_associativity);
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
