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
		snprintf(object->hdr.name, HIOBJNAMELEN, "cache:%s:%d",
				HI_CPU_PARENT_OBJ(object)->name, object->id);
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

	hi_for_each_child(child, parent) {
		cpu_object = HI_CPU_FROM_OBJ(child->object);

		/* Ignore caches */
		if(cpu_object->type == HI_CPU_CACHE) {
			continue;
		}

		if(cpu_object->type == HI_CPU_STRAND) {
			cpumask_set(mask, object->id);
			continue;
		}

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

const char* json_hi_cpu_format_type(struct hi_object_header* obj) {
	hi_cpu_object_t* cpuobj = HI_CPU_FROM_OBJ(obj);

	switch(cpuobj->type) {
	case HI_CPU_NODE:
		return "node";
	case HI_CPU_CHIP:
		return "chip";
	case HI_CPU_CORE:
		return "core";
	case HI_CPU_STRAND:
		return "strand";
	case HI_CPU_CACHE:
		return "cache";
	}

	return "unknown";
}

static const char* json_hi_cpu_format_cache_type(hi_cpu_cache_type_t type) {
	switch(type) {
	case HI_CPU_CACHE_UNIFIED:
		return "unified";
	case HI_CPU_CACHE_DATA:
		return "data";
	case HI_CPU_CACHE_INSTRUCTION:
		return "instruction";
	}

	return "unknown";
}

JSONNODE* json_hi_cpu_format(struct hi_object_header* obj) {
	JSONNODE* jcpu = json_new(JSON_NODE);
	hi_cpu_object_t* cpuobj = HI_CPU_FROM_OBJ(obj);

	json_push_back(jcpu, json_new_i("id", cpuobj->id));

	switch(cpuobj->type) {
	case HI_CPU_NODE:
		json_push_back(jcpu, json_new_i("mem_total", cpuobj->node.cm_mem_total));
		json_push_back(jcpu, json_new_i("mem_free", cpuobj->node.cm_mem_free));
		break;
	case HI_CPU_CHIP:
		json_push_back(jcpu, json_new_a("name", cpuobj->chip.cp_name));
		json_push_back(jcpu, json_new_i("frequency", cpuobj->chip.cp_freq));
		break;
	case HI_CPU_CACHE:
		json_push_back(jcpu, json_new_i("level", cpuobj->cache.c_level));
		json_push_back(jcpu, json_new_i("size", cpuobj->cache.c_size));
		json_push_back(jcpu, json_new_a("cache_type", json_hi_cpu_format_cache_type(cpuobj->cache.c_type)));
		json_push_back(jcpu, json_new_i("line_size", cpuobj->cache.c_line_size));
		json_push_back(jcpu, json_new_i("associativity", cpuobj->cache.c_associativity));
		break;
	}

	return jcpu;
}
