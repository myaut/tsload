/*
 * cpuinfo.h
 *
 *  Created on: 02.05.2013
 *      Author: myaut
 */

#ifndef CPUINFO_H_
#define CPUINFO_H_

#include <defs.h>
#include <list.h>

#include <libjson.h>

#include <hiobject.h>

#define 	HI_CPU_ANY			-1
#define 	HI_CPU_UNUSED		-2

#define HI_CPU_CACHEID(level, type)		(level << 2 | type)

typedef enum {
	HI_CPU_CACHE_UNIFIED,
	HI_CPU_CACHE_DATA,
	HI_CPU_CACHE_INSTRUCTION,
} hi_cpu_cache_type_t;

typedef enum {
	HI_CPU_NODE,
	HI_CPU_CHIP,
	HI_CPU_CORE,
	HI_CPU_STRAND,
	HI_CPU_CACHE
} hi_cpu_objtype_t;

typedef struct {
	uint32_t nodes;

	uint32_t mem_total;
	uint32_t mem_free;
} hi_cpu_stat_t;

#define HICPUNAMELEN	64

typedef struct {
	uint64_t cm_mem_total;
	uint64_t cm_mem_free;
} hi_cpu_node_t;

typedef struct {
	char cp_name[HICPUNAMELEN];
	uint64_t cp_freq;
} hi_cpu_chip_t;

typedef struct {
	int unused;
} hi_cpu_core_t;

typedef struct {
	int unused;
} hi_cpu_strand_t;

typedef struct {
	int 		   		c_level;
	hi_cpu_cache_type_t c_type;
	uint32_t	   		c_size;
	int 				c_associativity;
	int					c_line_size;
} hi_cpu_cache_t;

typedef struct hi_cpu_object {
	hi_object_header_t		hdr;

	int 					id;
	hi_cpu_objtype_t 		type;

	union {
		hi_cpu_node_t node;
		hi_cpu_chip_t chip;
		hi_cpu_core_t core;
		hi_cpu_strand_t strand;
		hi_cpu_cache_t cache;
	};
} hi_cpu_object_t;

#define HI_CPU_FROM_OBJ(object)		((hi_cpu_object_t*) (object))
#define HI_CPU_PARENT_OBJ(object)	object->hdr.node.parent
#define HI_CPU_PARENT(object)		HI_CPU_FROM_OBJ(HI_CPU_PARENT_OBJ(object))

PLATAPI int hi_cpu_probe(void);
void hi_cpu_dtor(hi_object_t* object);
int hi_cpu_init(void);
void hi_cpu_fini(void);

hi_cpu_object_t* hi_cpu_object_create(hi_cpu_object_t* parent, hi_cpu_objtype_t type, int id);
void hi_cpu_object_add(hi_cpu_object_t* object);
void* hi_cpu_find_byid(hi_cpu_object_t* parent, hi_cpu_objtype_t type, int id);

void hi_cpu_set_chip_name(hi_cpu_object_t* chip, const char* name);

STATIC_INLINE void hi_cpu_attach(hi_cpu_object_t* object, hi_cpu_object_t* parent) {
	hi_obj_attach((hi_object_header_t*) &object->hdr,
				  (hi_object_header_t*) &parent->hdr);
}

STATIC_INLINE void hi_cpu_detach(hi_cpu_object_t* object, hi_cpu_object_t* parent) {
	hi_obj_detach((hi_object_header_t*) &object->hdr,
				  (hi_object_header_t*) &parent->hdr);
}

/**
 * Find cpu object by it's name
 * @param name - name of disk
 *
 * @return disk descriptor or NULL if it wasn't found
 * */
STATIC_INLINE hi_cpu_object_t* hi_cpu_find(const char* name) {
	return   (hi_cpu_object_t*)	hi_obj_find(HI_SUBSYS_CPU, name);
}

STATIC_INLINE list_head_t* hi_cpu_list(boolean_t reprobe) {
	return hi_obj_list(HI_SUBSYS_CPU, reprobe);
}

LIBEXPORT int hi_cpu_num_cpus(void);
LIBEXPORT int hi_cpu_num_cores(void);

LIBEXPORT size_t hi_cpu_mem_total(void);

#ifndef NO_JSON
#include <libjson.h>

const char* json_hi_cpu_format_type(struct hi_object_header* obj);
JSONNODE* json_hi_cpu_format(struct hi_object_header* obj);
#endif

#include <hitrace.h>

#endif /* CPUINFO_H_ */
