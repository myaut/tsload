/*
 * cpuinfo.c
 *
 *  Created on: 02.05.2013
 *      Author: myaut
 */

#include <cpuinfo.h>
#include <mempool.h>
#include <hiobject.h>

/**
 * cpuinfo.c - reads information about cpus
 *
 * TODO: CPU chips
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
		object->node.cm_name[0] = '\0';
		object->node.cm_mem_total = 0;
		object->node.cm_mem_free = 0;
		object->node.cm_freq = 0;
	}
	else if(type == HI_CPU_CACHE) {
		object->cache.c_level = 0;
		object->cache.c_type = 0;
		object->cache.c_size = 0;
	}

	return object;
}

void hi_cpu_object_add(hi_cpu_object_t* object) {
	switch(object->type) {
	case HI_CPU_NODE:
		snprintf(object->hdr.name, HIOBJNAMELEN, "node:%d", object->id);
		break;
	case HI_CPU_CORE:
		snprintf(object->hdr.name, HIOBJNAMELEN, "core:%d:%d",
				HI_CPU_PARENT(object)->id, object->id);
		break;
	case HI_CPU_STRAND:
		snprintf(object->hdr.name, HIOBJNAMELEN, "strand:%d:%d:%d",
				HI_CPU_PARENT(object)->id, HI_CPU_PARENT(HI_CPU_PARENT(object))->id, object->id);
		break;
	case HI_CPU_CACHE:
		snprintf(object->hdr.name, HIOBJNAMELEN, "cache:%s:%d",
				HI_CPU_PARENT_OBJ(object)->name, object->id);
		break;
	}

	hi_obj_add(HI_SUBSYS_CPU, &object->hdr);
}

#if 0
#define HI_CPU_MATCH		0
#define HI_CPU_NEXT			1
#define HI_CPU_WALKTHRU		2

int hi_cpu_match(hi_cpu_object_t* obj, hi_cpu_objtype_t type, int id) {
	/* Need to find it deeper */
	if(type != HI_CPU_CACHE && type > obj->type)
		return HI_CPU_WALKTHRU;
	if(type == obj->type && id == obj->id)
		return HI_CPU_MATCH;
	return HI_CPU_NEXT;
}

switch(hi_cpu_match(object, type, id)) {
	case HI_CPU_WALKTHRU:
		result = hi_cpu_find(object, type, id);
		break;
	case HI_CPU_MATCH:
		result = object;
		break;
	}

	if(result)
		return (void*) result;
#endif

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

			return NULL;
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
	/* TODO: Should count CPU packages */
	return 0;
}

int hi_cpu_num_cores(void) {
	return hi_cpu_num_objs(HI_CPU_CORE);
}

size_t hi_cpu_mem_total(void) {
	/* TODO: Should sum all NUMA node mem_total values */
	return 0;
}

int hi_cpu_init(void) {
	mp_cache_init(&hi_cpu_obj_cache, hi_cpu_object_t);

	return 0;
}

void hi_cpu_fini(void) {
	mp_cache_destroy(&hi_cpu_obj_cache);
}
