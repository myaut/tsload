
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



#ifndef HIOBJECT_H_
#define HIOBJECT_H_

#include <tsload/defs.h>

#include <tsload/list.h>
#include <tsload/modules.h>
#include <tsload/pathutil.h>

#include <tsload/obj/obj.h>

HOSTINFOAPI char hi_obj_modpath[];

struct hi_object_header;

typedef enum {
	HI_SUBSYS_CPU,
	HI_SUBSYS_DISK,
	HI_SUBSYS_NET,
	HI_SUBSYS_FS,

	HI_SUBSYS_MAX
} hi_obj_subsys_id_t;

#define HI_PROBE_OK 			0
#define HI_PROBE_ERROR 			1
#define HI_PROBE_PROBING		2
#define HI_PROBE_NOT_PROBED		3

typedef int  (*hi_obj_probe_op)(void);
typedef void (*hi_obj_dtor_op)(struct hi_object_header* obj);
typedef int   (*hi_obj_init_op)(void);
typedef void  (*hi_obj_fini_op)(void);
typedef tsobj_node_t* (*hi_obj_tsobj_format_op)(struct hi_object_header* obj);

/**
 * HostInfo object subsystem operations
 * 
 * @member op_probe function that gets objects from OS and adds it to \
 *                  HostInfo heierarchy
 * @member op_dtor  destructor of HostInfo object that de-allocates   \
 *                  subsystem-specific fields i.e. strings
 * @member op_init  initialize subsystem (once)
 * @member op_fini  destroy subsystem (once)
 * @member op_tsobj_format  generates TSObject for HostInfo object
 */
typedef struct {
	hi_obj_probe_op		op_probe;
	hi_obj_dtor_op		op_dtor;
	hi_obj_init_op		op_init;
	hi_obj_fini_op		op_fini;

	hi_obj_tsobj_format_op 	op_tsobj_format;
} hi_obj_subsys_ops_t;

typedef struct {
	hi_obj_subsys_id_t	 id;
	char*				 name;
	hi_obj_subsys_ops_t* ops;
	list_head_t			 list;
	int					 state;
} hi_obj_subsys_t;

typedef struct {
	boolean_t loaded;
	
	AUTOSTRING char* path;
	plat_mod_library_t lib;
	
	hi_obj_probe_op	op_probe;
} hi_obj_helper_t;

#define HI_OBJ_SUBSYS(aid, aname, aops)					\
	{	SM_INIT(.id, aid),								\
		SM_INIT(.name, aname),							\
		SM_INIT(.ops, aops) }

#define HI_OBJ_CHILD_ROOT		0
#define HI_OBJ_CHILD_EMBEDDED	1
#define HI_OBJ_CHILD_ALLOCATED	2

typedef struct {
	struct hi_object_header* parent;
	struct hi_object_header* object;

	list_node_t	 	node;
	int				type;
} hi_object_child_t;

/**
 * Header of all HostInfo objects 
 * 
 * Should be included for all corresponding structures as first member
 * 
 * @member sid id of subsystem to which this HostInfo object relate
 * @member node storage for keeping child reference
 * @member list_node list node for global HostInfo object list
 * @member children head node for object's children
 * @member name unique name of object
 */
typedef struct hi_object_header {
	hi_obj_subsys_id_t			sid;
	hi_object_child_t			node;
	list_node_t					list_node;
	list_head_t 				children;

	unsigned short				ref_count;

	AUTOSTRING char*			name;
} hi_object_header_t;

typedef hi_object_header_t	hi_object_t;

void hi_obj_header_init(hi_obj_subsys_id_t sid, hi_object_header_t* hdr, const char* name);

LIBEXPORT void hi_obj_attach(hi_object_t* hdr, hi_object_t* parent);
LIBEXPORT void hi_obj_detach(hi_object_t* hdr, hi_object_t* parent);
LIBEXPORT void hi_obj_add(hi_obj_subsys_id_t sid, hi_object_t* object);
LIBEXPORT int hi_obj_destroy(hi_object_t* object);
LIBEXPORT void hi_obj_destroy_all(hi_obj_subsys_id_t sid);

LIBEXPORT hi_object_t* hi_obj_find(hi_obj_subsys_id_t sid, const char* name);

LIBEXPORT list_head_t* hi_obj_list(hi_obj_subsys_id_t sid, boolean_t reprobe);

LIBEXPORT int hi_obj_load_helper(hi_obj_helper_t* helper, const char* libname, const char* probefunc);
LIBEXPORT void hi_obj_unload_helper(hi_obj_helper_t* helper);

LIBEXPORT int hi_obj_init(void);
LIBEXPORT void hi_obj_fini(void);

LIBEXPORT tsobj_node_t* tsobj_hi_format_all(boolean_t reprobe);

/**
 * Iterate over object list or its children
 * 
 * `hi_for_each_object` and `hi_for_each_object_safe` take list returned by 
 * `hi_obj_list()` and its equivalent and return entries as `hi_object_t*`. 
 * 
 * `hi_for_each_child` and `hi_for_each_child_safe` take parent object represented 
 * as `hi_object_t*` and return entries of `hi_object_child_t*`.
 * 
 * @see list_for_each_entry, list_for_each_entry_safe
 * 
 * @note these macros are working with `hi_object_t` raw objects		\
 *       to access specific HostInfo object like CPU or disk, use subsystem \
 *       conversion operations
 */
#define hi_for_each_object(object, list)	\
		list_for_each_entry(hi_object_t, object, list, list_node)
#define hi_for_each_object_safe(object, next, list)	\
		list_for_each_entry_safe(hi_object_t, object, next, list, list_node)
#define hi_for_each_child(child, parent)	\
		list_for_each_entry(hi_object_child_t, child, &(parent)->children, node)
#define hi_for_each_child_safe(child, next, parent)	\
		list_for_each_entry_safe(hi_object_child_t, child, next, &(parent)->children, node)

#endif /* HIOBJECT_H_ */

