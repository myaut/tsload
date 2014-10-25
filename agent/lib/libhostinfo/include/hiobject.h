
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

#include <defs.h>
#include <list.h>

#include <tsobj.h>

struct hi_object_header;

/* Windows have long network interface names,
 * so provide large space for storing them */
#define HIOBJNAMELEN			128

typedef enum {
	HI_SUBSYS_CPU,
	HI_SUBSYS_DISK,
	HI_SUBSYS_NET,

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

typedef struct hi_object_header {
	hi_obj_subsys_id_t			sid;
	hi_object_child_t			node;
	list_node_t					list_node;
	list_head_t 				children;

	unsigned short				ref_count;

	char						name[HIOBJNAMELEN];
} hi_object_header_t;

typedef hi_object_header_t	hi_object_t;

void hi_obj_header_init(hi_obj_subsys_id_t sid, hi_object_header_t* hdr, const char* name);

LIBEXPORT void hi_obj_attach(hi_object_t* hdr, hi_object_t* parent);
LIBEXPORT void hi_obj_detach(hi_object_t* hdr, hi_object_t* parent);
LIBEXPORT void hi_obj_add(hi_obj_subsys_id_t sid, hi_object_t* object);
LIBEXPORT int hi_obj_destroy(hi_object_t* object);
LIBEXPORT void hi_dsk_destroy_all(hi_obj_subsys_id_t sid);

LIBEXPORT hi_object_t* hi_obj_find(hi_obj_subsys_id_t sid, const char* name);

LIBEXPORT list_head_t* hi_obj_list(hi_obj_subsys_id_t sid, boolean_t reprobe);

LIBEXPORT int hi_obj_init(void);
LIBEXPORT void hi_obj_fini(void);

LIBEXPORT tsobj_node_t* tsobj_hi_format_all(boolean_t reprobe);

#define hi_for_each_object(object, list)	\
		list_for_each_entry(hi_object_t, object, list, list_node)
#define hi_for_each_child(child, parent)	\
		list_for_each_entry(hi_object_child_t, child, &(parent)->children, node)

#define hi_for_each_object_safe(object, next, list)	\
		list_for_each_entry_safe(hi_object_t, object, next, list, list_node)
#define hi_for_each_child_safe(child, next, parent)	\
		list_for_each_entry_safe(hi_object_child_t, child, next, &(parent)->children, node)

#endif /* HIOBJECT_H_ */

