/*
 * cpuinfo.c
 *
 *  Created on: 03.05.2013
 *      Author: myaut
 */

#include <cpuinfo.h>
#include <ilog2.h>
#include <mempool.h>

#include <windows.h>

#define HI_WIN_MAXCPUS		64


/**
 * cpuinfo (Windows)
 *
 * TODO: CPU model
 * TODO: node memory
 */

/* Windows only provides numbers for "logical processors" (strands)
 * So we keep our own counter for cores.
 *
 * Also we have to cache strands values because we couldn't use
 * hi_cpu_find_byid befor we add strand into list */
struct hi_win_lproc_state {
	int core_count;

	hi_cpu_object_t* strands[HI_WIN_MAXCPUS];
};

static
uint64_t cpumask(hi_cpu_object_t* parent) {
	hi_object_child_t* child;
	hi_cpu_object_t* cpu_object;

	uint64_t mask = 0;

	hi_for_each_child(child, &parent->hdr) {
		cpu_object = HI_CPU_FROM_OBJ(child->object);

		if(cpu_object->type == HI_CPU_STRAND) {
			mask |= ((uint64_t) 1) << cpu_object->id;
		}
	}

	return mask;
}


void hi_win_proc_cache(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION lproc) {
	hi_cpu_cache_type_t cache_type;
	hi_cpu_object_t* cache;
	hi_cpu_object_t* node;
	hi_cpu_object_t* core;
	hi_cpu_object_t* parent;
	hi_cpu_object_t* strand = NULL;

	int strandid;
	int scount = sizeof(ULONG_PTR) * 8 - 1;

	hi_cpu_dprintf("hi_win_proc_lproc: found cache L%d:%d strand mask: %llx\n",
				   (int) lproc->Cache.Level, (int)  lproc->Cache.Type,
				   (unsigned long long) lproc->ProcessorMask);

	switch(lproc->Cache.Type) {
	case CacheUnified:
		cache_type = HI_CPU_CACHE_UNIFIED;
		break;
	case CacheInstruction:
		cache_type = HI_CPU_CACHE_INSTRUCTION;
		break;
	case CacheData:
		cache_type = HI_CPU_CACHE_DATA;
		break;
	case CacheTrace:
		/* Ignore */
		return;
	}

	/* Find first strand */
	for(strandid = 0; strandid < scount; ++strandid) {
		if((lproc->ProcessorMask) & (1 << strandid)) {
			strand = hi_cpu_find_byid(NULL, HI_CPU_STRAND, strandid);
			break;
		}
	}

	hi_cpu_dprintf("hi_win_proc_lproc: => strand %p\n", strand);

	if(strand == NULL)
		return;

	cache = hi_cpu_object_create(NULL, HI_CPU_CACHE, HI_CPU_CACHEID(lproc->Cache.Level, cache_type));
	cache->cache.c_level = lproc->Cache.Level;
	cache->cache.c_type = cache_type;
	cache->cache.c_size = lproc->Cache.Size;
	cache->cache.c_associativity = lproc->Cache.Associativity;
	cache->cache.c_line_size = lproc->Cache.LineSize;

	core = HI_CPU_PARENT(strand);
	node = HI_CPU_PARENT(core);

	parent = (cpumask(core) == lproc->ProcessorMask)? core : node;

	hi_cpu_attach(cache, parent);

	hi_cpu_object_add(cache);
}

/* Insert cores/strands into global list. In hi_win_proc_lproc we can only link nodes,
 * because other cpu object may not have parents in moment of creation
 *
 * After all logical processor relationships processed we build tree, but global list
 * has only nodes - so we should insert strands and cores too*/
void hi_win_add_cores_strands(void) {
	list_head_t* cpu_list = hi_cpu_list(B_FALSE);
	hi_object_t *object, *next;
	hi_cpu_object_t* node;

	hi_object_child_t *c_core, *c_strand;

	hi_for_each_object_safe(object, next, cpu_list) {
		node = HI_CPU_FROM_OBJ(object);

		if(node->type != HI_CPU_NODE)
			break;

		hi_for_each_child(c_core, object) {
			hi_cpu_object_add(HI_CPU_FROM_OBJ(c_core->object));

			hi_for_each_child(c_strand, c_core->object) {
				hi_cpu_object_add(HI_CPU_FROM_OBJ(c_strand->object));
			}
		}
	}
}

void hi_win_proc_lproc(struct hi_win_lproc_state* state, PSYSTEM_LOGICAL_PROCESSOR_INFORMATION lproc) {
	hi_cpu_object_t* object;
	hi_cpu_object_t* parent;
	hi_cpu_object_t* strand;

	int strandid;
	int scount = sizeof(ULONG_PTR) * 8 - 1;

	switch(lproc->Relationship) {
	case RelationNumaNode:
		object = hi_cpu_object_create(NULL, HI_CPU_NODE, lproc->NumaNode.NodeNumber);
		hi_cpu_dprintf("hi_win_proc_lproc: found numa node #%d mask: %llx\n",
					   lproc->NumaNode.NodeNumber, (unsigned long long) lproc->ProcessorMask);
		break;
	case RelationProcessorCore:
		object = hi_cpu_object_create(NULL, HI_CPU_CORE, state->core_count++);
		hi_cpu_dprintf("hi_win_proc_lproc: found core mask: %llx\n", (unsigned long long) lproc->ProcessorMask);
		break;
	default:
		/* Ignore */
		return;
	}

	/* FIXME: For systems with more than 64 processors this wouldn't work */
	for(strandid = 0; strandid < scount; ++strandid) {
		if(!((lproc->ProcessorMask) & (1 << strandid)))
			continue;

		strand = state->strands[strandid];
		if(strand == NULL) {
			strand = hi_cpu_object_create(NULL, HI_CPU_STRAND, strandid);
			state->strands[strandid] = strand;
		}

		parent = HI_CPU_PARENT(strand);

		switch(lproc->Relationship) {
		case RelationNumaNode:
			if(parent) {
				/* Logical processor already have core,
				 * link core to this NUMA node */
				hi_cpu_attach(parent, object);
			}
			else {
				/* Temporarily link strand directly to NUMA node */
				hi_cpu_attach(strand, object);
			}
			hi_cpu_object_add(object);
			break;
		case RelationProcessorCore:
			if(parent != NULL && parent->type == HI_CPU_NODE) {
				/* Link core to NUMA node */
				hi_cpu_attach(object, parent);
			}
			hi_cpu_attach(strand, object);
			break;
		}
	}
}

PLATAPI int hi_cpu_probe(void) {
	BOOL status;
	int error;
	boolean_t allocated = B_FALSE;

	struct hi_win_lproc_state state;

	SYSTEM_LOGICAL_PROCESSOR_INFORMATION static_buf[8];
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buf = static_buf;

	DWORD buf_len = sizeof(static_buf);
	int i, count;

	state.core_count = 0;
	for(i = 0; i < HI_WIN_MAXCPUS; ++i)
		state.strands[i] = NULL;

	/* hostinfo is usually built from sources - no need for LoadLibrary stuff */
	status = GetLogicalProcessorInformation(buf, &buf_len);

	if(!status) {
		error = GetLastError();
		if(error == ERROR_INSUFFICIENT_BUFFER) {
			buf = mp_malloc(buf_len);
			allocated = B_TRUE;

			status = GetLogicalProcessorInformation(buf, &buf_len);

			if(!status) {
				/* Internal error - exit */
				mp_free(buf);
				return HI_PROBE_ERROR;
			}
		}
		else {
			return HI_PROBE_ERROR;
		}
	}

	count = buf_len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

	hi_cpu_dprintf("hi_cpu_probe: found %d lproc entities\n", count);

	for(i = 0; i < count; ++i) {
		hi_win_proc_lproc(&state, &buf[i]);
	}

	hi_win_add_cores_strands();

	for(i = 0; i < count; ++i) {
		if(buf[i].Relationship == RelationCache)
			hi_win_proc_cache(&buf[i]);
	}

	if(allocated)
		mp_free(buf);

	return HI_PROBE_OK;
}
