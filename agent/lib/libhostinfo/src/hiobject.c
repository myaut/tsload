
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



#include <hiobject.h>
#include <mempool.h>
#include <cpuinfo.h>
#include <diskinfo.h>
#include <hitrace.h>

#include <string.h>
#include <assert.h>

/**
 * HostInfo Object - abstract host object container implementation
 *
 * Host object may be a device, filesystem or network interface - so
 * generally speaking it is any entity that could be utilized by loader or monitor.
 *
 * Objects are split into several subsystems:
 * - CPU - cpus, cores, strands, NUMA nodes with memory
 * - DISK - hard disks and SSDs, partitions, volumes and pools of volumes
 * - FS - filesystems
 * - NET - NICs and network interfaces
 *
 * hi_obj_subsys contains global table of implemented subsystems. Each subsystem has
 * global list of objects (which contains not only root objects) accessible by hi_obj_list.
 *
 * Each object represented by implementation-specific structure, but each begins
 * with header hi_object_header_t. Also, each subsystem implements their own object
 * methods which proxy requests to hi_object, and custom operations:
 *
 * hi_dsk_list(...) -> hi_obj_list(HI_SUBSYS_DISK, ...) -> subsys->ops->op_probe [hi_dsk_probe]
 *
 * Objects are forming a hierarchy: each object may have multiple parents (but refer
 * only to first of them, usually object has only one parent, so this is enough) and
 * multiple children. Every children reference is represented by  hi_object_child_t structure.
 * First child node is embedded into its object, if multiple parents are exist, it is
 * allocated from heap.
 *
 * Object lifetime:
 * 1. hi_*_create - creates new object (implemented by subsystem)
 *       -> hi_obj_header_init
 *       -> hi_obj_child_init
 *   (Subsystem fills in object information)
 * 2. hi_*_add
 *       (Creates object name)
 * 	     -> hi_obj_add
 *
 * 3 (optional). hi_obj_attach
 *
 * ...
 *
 * n. hi_obj_destroy
 * 		-> subsys->ops->op_dtor
 *
 * When list of objects is accessed for first time, we call subsys->op->op_probe
 * to read it from operating system. Current subsystem state is identified by subsys->state field:
 *
 *               hi_obj_list            op_probe
 *  NOT_PROBED -------------> PROBING -----------> OK -------+
 *      ^                               |                    |
 *      |                               +--------> ERROR ----+
 *      |                                                    |
 *      +----------------------------------------------------+
 *                  hi_obj_destroy_all || reprobe
 */

hi_obj_subsys_ops_t cpu_ops = {
	hi_cpu_probe,
	hi_cpu_dtor,
	hi_cpu_init,
	hi_cpu_fini,
	tsobj_hi_cpu_format
};

hi_obj_subsys_ops_t disk_ops = {
	hi_dsk_probe,
	hi_dsk_dtor,
	hi_dsk_init,
	hi_dsk_fini,
	tsobj_hi_dsk_format
};

hi_obj_subsys_t hi_obj_subsys[HI_SUBSYS_MAX] =
{
	HI_OBJ_SUBSYS(HI_SUBSYS_CPU, "cpu", &cpu_ops),
	HI_OBJ_SUBSYS(HI_SUBSYS_DISK, "disk", &disk_ops),
};

/* Last initialized subsystem. Needed for correct finish if hi_obj_init fails. */
static int max_subsys = 0;

static hi_obj_subsys_t* get_subsys(hi_obj_subsys_id_t sid) {
	if(sid >= HI_SUBSYS_MAX)
		return NULL;

	return &hi_obj_subsys[sid];
}

static
void hi_obj_child_init(hi_object_child_t* child, hi_object_header_t* object) {
	child->type = HI_OBJ_CHILD_ROOT;

	child->parent = NULL;
	child->object = object;

	list_node_init(&child->node);
}

/**
 * Initialize object header
 * @param sid		subsystem identifier
 * @param hdr		object header
 * @param name		default object name
 * */
void hi_obj_header_init(hi_obj_subsys_id_t sid, hi_object_header_t* hdr, const char* name) {
	hdr->sid = sid;

	strncpy(hdr->name, name, HIOBJNAMELEN);

	list_head_init(&hdr->children, "hiobj_children");

	hi_obj_child_init(&hdr->node, hdr);
	list_node_init(&hdr->list_node);

	hdr->ref_count = 0;
}

/**
 * Attach object hdr to parent
 * @param object	object to attach
 * @param parent 	parent object
 */
void hi_obj_attach(hi_object_t* object, hi_object_t* parent) {
	hi_object_child_t* child;

	if(object->node.parent == NULL) {
		/* This is first parent for this object, use embedded node */
		child = &object->node;
		child->type = HI_OBJ_CHILD_EMBEDDED;
	}
	else {
		/* Need to allocate new child node */
		child = (hi_object_child_t*) mp_malloc(sizeof(hi_object_child_t));
		child->type = HI_OBJ_CHILD_ALLOCATED;
	}

	child->object = object;
	child->parent = parent;

	hi_obj_dprintf("hi_obj_attach: %p %d %s->%s\n", child, child->type,
					parent->name, object->name);

	list_add_tail(&child->node, &parent->children);

	++object->ref_count;
}

/**
 * Detach object from parent
 * @param object	object to detach
 * @param parent 	parent object
 * */
void hi_obj_detach(hi_object_t* object, hi_object_t* parent) {
	hi_object_child_t *child = NULL;

	assert(object->ref_count > 0);

	/* Ensure that object is really child of that parent */
	hi_for_each_child(child, parent) {
		if(child->object == object)
			break;
	}

	assert(child != NULL);

	list_del(&child->node);
	child->parent = NULL;

	/* Free child handler if the were allocated */
	if(child->type == HI_OBJ_CHILD_ALLOCATED)
		mp_free(child);

	--object->ref_count;
}

/**
 * Destroy object and its children recursively (if no more parents
 * for this child left). If object is not root (has parents), doesn't
 * destroy anything and returns 1, otherwise returns 0.
 *
 * Frees child handlers then calls subsystem-specific destructor
 */
int hi_obj_destroy(hi_object_t* object) {
	hi_object_child_t *child, *next;
	hi_obj_subsys_t* subsys;

	boolean_t allocated_child = B_FALSE;

	if(object->ref_count > 0)
		return 1;

	list_del(&object->list_node);

	hi_for_each_child_safe(child, next, object) {
		hi_obj_dprintf("hi_obj_destroy: child %p %p %d %s\n", child, next,
							child->type, child->object->name);

		allocated_child = TO_BOOLEAN(child->type == HI_OBJ_CHILD_ALLOCATED);

		if(--child->object->ref_count == 0) {
			/* We were last parent of that child */
			hi_obj_destroy(child->object);
		}

		if(allocated_child)
			mp_free(child);
	}

	subsys = get_subsys(object->sid);
	subsys->ops->op_dtor(object);

	return 0;
}

/**
 * Destroy all objects for subsystem sid
 * @param sid		subsystem identifier
 */
void hi_obj_destroy_all(hi_obj_subsys_id_t sid) {
	hi_object_t* object;
	hi_obj_subsys_t* subsys = get_subsys(sid);

	if(!subsys || subsys->state == HI_PROBE_NOT_PROBED)
		return;

	while(!list_empty(&subsys->list)) {
		hi_for_each_object(object, &subsys->list) {
			/* If we successfully destroyed objects - next pointer
			 * could be also corrupted (if it was child) - so reset
			 * for-each cycle */
			if(hi_obj_destroy(object) == 0)
				break;
		}
	}

	list_head_init(&subsys->list, "hi_%s_list", subsys->name);

	subsys->state = HI_PROBE_NOT_PROBED;
}

/**
 * Add object to global list
 * */
void hi_obj_add(hi_obj_subsys_id_t sid, hi_object_t* object) {
	hi_obj_subsys_t* subsys = get_subsys(sid);

	if(subsys)
		list_add_tail(&object->list_node, &subsys->list);
}

/**
 * Find object by it's name
 * @param sid 	subsystem identifier
 * @param name	object name
 *
 * TODO: hashmap cache?
 * */
hi_object_t* hi_obj_find(hi_obj_subsys_id_t sid, const char* name) {
	hi_object_t* object;
	list_head_t* list;

	list = hi_obj_list(sid, B_FALSE);

	if(!list)
		return NULL;

	hi_for_each_object(object, list) {
		if(strcmp(object->name, name) == 0)
			return object;
	}

	return NULL;
}

/**
 * Get list head of hostinfo object list
 * @param sid 		subsystem identifier
 * @param reprobe 	reread objects from operating system
 *
 * @return list head or NULL if probe error occured, or subsystem identifier
 * is invalid
 * */
list_head_t* hi_obj_list(hi_obj_subsys_id_t sid, boolean_t reprobe) {
	hi_obj_subsys_t* subsys = get_subsys(sid);

	if(!subsys)
		return NULL;

	if(reprobe) {
		hi_obj_destroy_all(sid);
	}

	/* State machine (see comment on top of source) */
probe:
	switch(subsys->state) {
	case HI_PROBE_NOT_PROBED:
		subsys->state = HI_PROBE_PROBING;
		subsys->state = subsys->ops->op_probe();
		goto probe;
	case HI_PROBE_OK:
	case HI_PROBE_PROBING:
		return &subsys->list;
	case HI_PROBE_ERROR:
		return NULL;
	}

	/* NOTREACHED */
	return NULL;
}

int hi_obj_init(void) {
	int sid = 0;
	int ret = 0;

	hi_obj_subsys_t* subsys = NULL;

	for(sid = 0; sid < HI_SUBSYS_MAX; ++sid) {
		subsys = get_subsys(sid);

		list_head_init(&subsys->list, "hi_%s_list", subsys->name);
		subsys->state = HI_PROBE_NOT_PROBED;

		ret = subsys->ops->op_init();

		if(ret != 0)
			break;
	}

	max_subsys = sid - 1;

	return ret;
}

void hi_obj_fini(void) {
	int sid = 0;

	for(sid = max_subsys; sid >= 0; --sid) {
		hi_obj_destroy_all(sid);

		get_subsys(sid)->ops->op_fini();
	}
}

tsobj_node_t* tsobj_hi_format_all(boolean_t reprobe) {
	tsobj_node_t* hiobj = tsobj_new_node(NULL);

	int sid = 0;
	hi_obj_subsys_t* subsys = NULL;

	list_head_t* list;
	hi_object_t* object;
	hi_object_child_t *child = NULL;

	tsobj_node_t* o_subsys;
	tsobj_node_t* o_object;
	tsobj_node_t* o_children;

	for(sid = 0; sid < HI_SUBSYS_MAX; ++sid) {
		subsys = get_subsys(sid);
		list = hi_obj_list(sid, reprobe);

		o_subsys = tsobj_new_node(NULL);
		tsobj_add_node(hiobj, tsobj_str_create(subsys->name), o_subsys);

		hi_for_each_object(object, list) {
			o_object = tsobj_new_node(NULL);
			tsobj_add_node(o_subsys, tsobj_str_create(object->name), o_object);

			o_children = json_new_array();

			hi_for_each_child(child, object) {
				tsobj_add_string(o_children, NULL, tsobj_str_create(child->object->name));
			}

			tsobj_add_node(o_object, TSOBJ_STR("children"), o_children);

			tsobj_add_node(o_object, TSOBJ_STR("data"),
					subsys->ops->op_tsobj_format(object));
		}
	}

	return hiobj;
}

