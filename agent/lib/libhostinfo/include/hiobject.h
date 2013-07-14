/*
 * hiobject.h
 *
 *  Created on: 03.05.2013
 *      Author: myaut
 */

#ifndef HIOBJECT_H_
#define HIOBJECT_H_

#include <defs.h>
#include <list.h>

struct hi_object_header;

#define HIOBJNAMELEN			48

typedef enum {
	HI_SUBSYS_CPU,
	HI_SUBSYS_DISK,

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

typedef struct {
	hi_obj_probe_op		op_probe;
	hi_obj_dtor_op		op_dtor;
	hi_obj_init_op		op_init;
	hi_obj_fini_op		op_fini;
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

	char						name[HIOBJNAMELEN];
} hi_object_header_t;

typedef hi_object_header_t	hi_object_t;

void hi_obj_header_init(hi_obj_subsys_id_t sid, hi_object_header_t* hdr, const char* name);

LIBEXPORT void hi_obj_attach(hi_object_t* hdr, hi_object_t* parent);
LIBEXPORT void hi_obj_detach(hi_object_t* hdr, hi_object_t* parent);
LIBEXPORT void hi_obj_add(hi_obj_subsys_id_t sid, hi_object_t* object);
LIBEXPORT void hi_obj_destroy(hi_object_t* object);
LIBEXPORT void hi_dsk_destroy_all(hi_obj_subsys_id_t sid);

LIBEXPORT hi_object_t* hi_obj_find(hi_obj_subsys_id_t sid, const char* name);

LIBEXPORT list_head_t* hi_obj_list(hi_obj_subsys_id_t sid, boolean_t reprobe);

LIBEXPORT int hi_obj_init(void);
LIBEXPORT void hi_obj_fini(void);

#define hi_for_each_object(object, list)	\
		list_for_each_entry(hi_object_t, object, list, list_node)
#define hi_for_each_child(child, parent)	\
		list_for_each_entry(hi_object_child_t, child, &(parent)->children, node)

#define hi_for_each_object_safe(object, next, list)	\
		list_for_each_entry_safe(hi_object_t, object, next, list, list_node)
#define hi_for_each_child_safe(child, next, parent)	\
		list_for_each_entry_safe(hi_object_child_t, child, next, &(parent)->children, node)

#endif /* HIOBJECT_H_ */
