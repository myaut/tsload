/*
 * cpuinfo.c
 *
 *  Created on: 02.05.2013
 *      Author: myaut
 */

#include <cpuinfo.h>
#include <mempool.h>
#include <hiobject.h>
#include <cpumask.h>

#include <ctype.h>

/**
 * cpuinfo.c - reads information about cpus
 * */

mp_cache_t hi_cpu_obj_cache;

hi_cpu_object_t* hi_cpu_object_create(hi_cpu_object_t* parent, hi_cpu_objtype_t type, int id) {
	hi_cpu_object_t* object = (hi_cpu_object_t*) mp_cache_alloc(&hi_cpu_obj_cache);

	hi_obj_header_init(HI_SUBSYS_CPU, &object->hdr, "cpuobj");
	if(parent)
		hi_obj_attach(&object->hdr, &parent->hdr);

	object->type = type;
	object->id = id;

	if(type == HI_CPU_NODE) {
		object->node.cm_mem_total = 0;
		object->node.cm_mem_free = 0;
	}
	else if(type == HI_CPU_CHIP) {
		object->chip.cp_name[0] = '\0';
		object->chip.cp_freq = 0;
	}
	else if(type == HI_CPU_CACHE) {
		object->cache.c_level = 0;
		object->cache.c_type = 0;
		object->cache.c_size = 0;
	}

	return object;
}

void hi_cpu_set_chip_name(hi_cpu_object_t* chip, const char* name) {
	boolean_t wasspace = B_FALSE;
	char* p = name;
	int i = 0;

	/* Model names may contain (R), (TM) or continously going spaces, ignore them */
	while(*p && i < HICPUNAMELEN) {
		if(wasspace && isspace(*p)) {
			p++;
			continue;
		}
		if(strncmp(p, "(R)", 3) == 0 ||
		   strncmp(p, "(r)", 3) == 0)
			p += 3;
		if(strncmp(p, "(TM)", 4) == 0 ||
		   strncmp(p, "(TM)", 4) == 0)
			p += 4;

		wasspace = TO_BOOLEAN(isspace(*p));

		chip->chip.cp_name[i++] = *p++;
	}

	chip->chip.cp_name[i] = '\0';
}

void hi_cpu_object_add(hi_cpu_object_t* object) {
	switch(object->type) {
	case HI_CPU_NODE:
		snprintf(object->hdr.name, HIOBJNAMELEN, "node:%d", object->id);
		break;
	case HI_CPU_CHIP:
		snprintf(object->hdr.name, HIOBJNAMELEN, "chip:%d", object->id);
		break;
	case HI_CPU_CORE:
		snprintf(object->hdr.name, HIOBJNAMELEN, "core:%d:%d",
				HI_CPU_PARENT(object)->id, object->id);
		break;
	case HI_CPU_STRAND:
		snprintf(object->hdr.name, HIOBJNAMELEN, "strand:%d:%d:%d",
				HI_CPU_PARENT(HI_CPU_PARENT(object))->id, HI_CPU_PARENT(object)->id, object->id);
		break;
	case HI_CPU_CACHE:
		{
			hi_cpu_object_t* cache = HI_CPU_FROM_OBJ(object);
			if(cache->cache.c_level != HI_CPU_CACHE_TLB_LEVEL) {
				snprintf(object->hdr.name, HIOBJNAMELEN, "cache:l%d:%d",
						cache->cache.c_level, object->id);
			}
			else {
				snprintf(object->hdr.name, HIOBJNAMELEN, "cache:tlb:%d", object->id);
			}
		}
		break;
	}

	hi_obj_add(HI_SUBSYS_CPU, &object->hdr);
}

/**
 * Find CPU object by it's type and id. More confortable than searching by object name.
 * Walks global list, than walk parents if needed
 *
 * If parent == NULL, parents aren't checked
 * */
void* hi_cpu_find_byid(hi_cpu_object_t* parent, hi_cpu_objtype_t type, int id) {
	hi_object_t *object;
	hi_cpu_object_t *cpu_object, *obj_cpu_parent;

	list_head_t* list = hi_cpu_list(B_FALSE);

	hi_for_each_object(object, list) {
		cpu_object = HI_CPU_FROM_OBJ(object);

		if(cpu_object->type == type && cpu_object->id == id) {
			/* Parent irrelevant - return immediately */
			if(parent == NULL)
				return cpu_object;

			obj_cpu_parent = HI_CPU_PARENT(cpu_object);

			do {
				if(obj_cpu_parent == parent)
					return cpu_object;

				obj_cpu_parent = HI_CPU_PARENT(obj_cpu_parent);
			} while(obj_cpu_parent != NULL);

			/* Not of this parent - try next objects */
			continue;
		}
	}

	return NULL;
}

void hi_cpu_dtor(hi_object_t* object) {
	hi_cpu_object_t* cpu_object = (hi_cpu_object_t*) object;
	mp_cache_free(&hi_cpu_obj_cache, cpu_object);
}

static
int hi_cpu_num_objs(hi_cpu_objtype_t obj_type) {
	int count = 0;

	list_head_t* cpu_list = hi_cpu_list(B_FALSE);
	hi_object_t*  object;
	hi_cpu_object_t* node;

	hi_for_each_object(object, cpu_list) {
		node = HI_CPU_FROM_OBJ(object);

		if(node->type == obj_type)
			++count;
	}

	return count;
}

int hi_cpu_num_cpus(void) {
	return hi_cpu_num_objs(HI_CPU_CHIP);
}

int hi_cpu_num_cores(void) {
	return hi_cpu_num_objs(HI_CPU_CORE);
}

size_t hi_cpu_mem_total(void) {
	/* TODO: Should sum all NUMA node mem_total values */
	return 0;
}

static int hi_cpu_mask_all(cpumask_t* mask) {
	hi_object_t *object;
	hi_cpu_object_t *cpu_object;

	list_head_t* list = hi_cpu_list(B_FALSE);

	hi_for_each_object(object, list) {
		cpu_object = HI_CPU_FROM_OBJ(object);

		if(cpu_object->type == HI_CPU_STRAND) {
			cpumask_set(mask, cpu_object->id);
		}
	}

	return 0;
}

static int hi_cpu_mask_impl(hi_cpu_object_t* object, cpumask_t* mask) {
	hi_object_t* parent = HI_CPU_TO_OBJ(object);
	hi_object_child_t* child;
	hi_cpu_object_t* cpu_object;

	/* Ignore caches */
	if(object->type == HI_CPU_CACHE) {
		return 1;
	}

	if(object->type == HI_CPU_STRAND) {
		cpumask_set(mask, object->id);
		return 0;
	}

	hi_for_each_child(child, parent) {
		cpu_object = HI_CPU_FROM_OBJ(child->object);

		/* Recursively call */
		hi_cpu_mask_impl(cpu_object, mask);
	}

	return 0;
}

/**
 * Create cpumask for cpu object
 *
 * For caches processes nearest core/chip object (parent)
 * For other cpu objects - processes all child strands
 * For null creates mask that includes all strands*/
int hi_cpu_mask(hi_cpu_object_t* object, cpumask_t* mask) {
	if(object == NULL)
		return hi_cpu_mask_all(mask);

	if(object->type == HI_CPU_CACHE)
		return hi_cpu_mask_impl(HI_CPU_PARENT(object), mask);

	return hi_cpu_mask_impl(object, mask);
}


int hi_cpu_init(void) {
	mp_cache_init(&hi_cpu_obj_cache, hi_cpu_object_t);

	return 0;
}

void hi_cpu_fini(void) {
	mp_cache_destroy(&hi_cpu_obj_cache);
}

static tsobj_str_t tsobj_hi_cpu_format_cache_type(hi_cpu_cache_type_t type) {
    switch(type) {
    case HI_CPU_CACHE_UNIFIED:
        return TSOBJ_STR("unified");
    case HI_CPU_CACHE_DATA:
        return TSOBJ_STR("data");
    case HI_CPU_CACHE_INSTRUCTION:
        return TSOBJ_STR("instruction");
    }

    return TSOBJ_STR("unknown");
}

tsobj_node_t* tsobj_hi_cpu_format(struct hi_object_header* obj) {
	tsobj_node_t* cpu;
	hi_cpu_object_t* cpuobj = HI_CPU_FROM_OBJ(obj);

	switch(cpuobj->type) {
	case HI_CPU_NODE:
		cpu = tsobj_new_node("tsload.hi.CPUNode");

		tsobj_add_integer(cpu, TSOBJ_STR("mem_total"), cpuobj->node.cm_mem_total);
		tsobj_add_integer(cpu, TSOBJ_STR("mem_free"), cpuobj->node.cm_mem_free);
		break;
	case HI_CPU_CHIP:
		cpu = tsobj_new_node("tsload.hi.CPUChip");

		tsobj_add_string(cpu, TSOBJ_STR("name"), tsobj_str_create(cpuobj->chip.cp_name));
		tsobj_add_integer(cpu, TSOBJ_STR("frequency"), cpuobj->chip.cp_freq);
		break;
	case HI_CPU_CACHE:
		cpu = tsobj_new_node("tsload.hi.CPUCache");

		if(cpuobj->cache.c_level != HI_CPU_CACHE_TLB_LEVEL) {
			tsobj_add_integer(cpu, TSOBJ_STR("line_size"), cpuobj->cache.c_unit_size.line);
			tsobj_add_integer(cpu, TSOBJ_STR("level"), cpuobj->cache.c_level);
		}
		else {
			/* TODO: Implement TLBs */
		}
		tsobj_add_integer(cpu, TSOBJ_STR("size"), cpuobj->cache.c_size);
		tsobj_add_string(cpu, TSOBJ_STR("cache_type"),
				tsobj_hi_cpu_format_cache_type(cpuobj->cache.c_type));
		tsobj_add_integer(cpu, TSOBJ_STR("associativity"), cpuobj->cache.c_associativity);
		break;
	case HI_CPU_CORE:
		cpu = tsobj_new_node("tsload.hi.CPUCore");
		break;
	case HI_CPU_STRAND:
		cpu = tsobj_new_node("tsload.hi.CPUStrand");
		break;
	}

	tsobj_add_integer(cpu, TSOBJ_STR("id"), cpuobj->id);

	return cpu;
}
