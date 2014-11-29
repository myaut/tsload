
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



#include <tsload/defs.h>

#include <tsload/mempool.h>

#include <hostinfo/cpuinfo.h>
#include <hostinfo/plat/minipicl.h>

#include <hitrace.h>

#include <string.h>

#include <kstat.h>
#include <sys/lgrp_user.h>



/**
 * cpuinfo (Solaris)
 *
 * Uses liblgrp to gather NUMA nodes, kstat to collect information about CPUs and
 * libpicl to find caches.
 *
 * libpicl only operates with processor instances, so there is hard to determine if
 * cache is assigned to chip or core. So code uses simple assumption: it compares 'cache_id'
 * from kstat within chip. If it differs, last level cache is individual for cores,
 * otherwise it is shared between them.
 */

#define LGRP_MAX_CPUS		64
#define LGRP_MAX_CHILDREN	64

#define HI_SOL_CPU_CACHE(level, type)			\
		{	SM_INIT( .c_level, level ), 		\
			SM_INIT( .c_type, type ),			\
			SM_INIT( .c_size, 0 ),				\
			SM_INIT( .c_associativity, 0 ),		\
			SM_INIT( .c_unit_size.line, 0 ) }

/* Do not use SM_INIT for page sizes because it have curly braces
 * which are breaking gcc preprocessor */
#define HI_SOL_CPU_TLB(type, ...)		\
		{	SM_INIT( .c_level, HI_CPU_CACHE_TLB_LEVEL ), 		\
			SM_INIT( .c_type, type ),			\
			SM_INIT( .c_size, 0 ),				\
			SM_INIT( .c_associativity, 0 ),		\
			.c_unit_size.page = { __VA_ARGS__ , 0 } }

struct hi_cpu_sol_cache {
	char* 			    prefix;
	boolean_t			found;
	hi_cpu_cache_t		cache;
};

struct hi_cpu_sol_cache solaris_caches[] =
{
	/* TLB */
	{ "itlb-4K", B_FALSE, HI_SOL_CPU_TLB(HI_CPU_CACHE_INSTRUCTION, 4 * SZ_KB) },
	{ "dtlb-4K", B_FALSE, HI_SOL_CPU_TLB(HI_CPU_CACHE_DATA, 4 * SZ_KB) },
	{ "itlb-2M", B_FALSE, HI_SOL_CPU_TLB(HI_CPU_CACHE_INSTRUCTION, 2 * SZ_MB) },
	{ "itlb-2M-4M", B_FALSE, HI_SOL_CPU_TLB(HI_CPU_CACHE_INSTRUCTION, 2 * SZ_MB, 4 * SZ_MB) },
	{ "itlb-4M", B_FALSE, HI_SOL_CPU_TLB(HI_CPU_CACHE_INSTRUCTION, 4 * SZ_MB) },
	{ "dtlb-2M", B_FALSE, HI_SOL_CPU_TLB(HI_CPU_CACHE_DATA, 2 * SZ_MB) },
	{ "dtlb0-2M-4M", B_FALSE, HI_SOL_CPU_TLB(HI_CPU_CACHE_DATA, 2 * SZ_MB, 4 * SZ_MB) },
	{ "itlb-4K-2M-4M", B_FALSE, HI_SOL_CPU_TLB(HI_CPU_CACHE_INSTRUCTION,
				4 * SZ_KB, 2 * SZ_MB, 4 * SZ_MB) },
	{ "dtlb-4K-4M", B_FALSE, HI_SOL_CPU_TLB(HI_CPU_CACHE_DATA, 4 * SZ_KB, 4 * SZ_MB) },

	/* FIXME: support for shared TLB/L2 caches i.e: *
	 *
	 * { "shared-l2-tlb-4k", B_FALSE, HI_SOL_CPU_TLB(HI_CPU_CACHE_INSTRUCTION, 4 * SZ_KB) },*/

	/* Caches */

	/* x86 & SPARC64 */
	{ "l1-icache", B_FALSE, HI_SOL_CPU_CACHE(1, HI_CPU_CACHE_INSTRUCTION) },
	{ "l1-dcache", B_FALSE, HI_SOL_CPU_CACHE(1, HI_CPU_CACHE_DATA) },
	{ "sectored-l1-cache", B_FALSE, HI_SOL_CPU_CACHE(1, HI_CPU_CACHE_UNIFIED) },

	{ "shared-l2-tlb-4k", B_FALSE, HI_SOL_CPU_CACHE(2, HI_CPU_CACHE_UNIFIED) },
	{ "l2-cache", B_FALSE, HI_SOL_CPU_CACHE(2, HI_CPU_CACHE_UNIFIED) },
	{ "sectored-l2-cache", B_FALSE, HI_SOL_CPU_CACHE(2, HI_CPU_CACHE_UNIFIED) },

	{ "l3-cache", B_FALSE, HI_SOL_CPU_CACHE(3, HI_CPU_CACHE_UNIFIED) },
	{ "sectored-l3-cache", B_FALSE, HI_SOL_CPU_CACHE(3, HI_CPU_CACHE_UNIFIED) },

	/* UltraSPARC */
	{ "icache", B_FALSE, HI_SOL_CPU_CACHE(1, HI_CPU_CACHE_INSTRUCTION) },
	{ "dcache", B_FALSE, HI_SOL_CPU_CACHE(1, HI_CPU_CACHE_DATA) },
	{ "ecache", B_FALSE, HI_SOL_CPU_CACHE(2, HI_CPU_CACHE_UNIFIED) },
};

struct cache_walk_args {
	hi_cpu_object_t* chip;
	processorid_t cpu;
	boolean_t shared_last_cache;
	boolean_t first_cpu;
};

/**
 * Taken from usr/src/lib/libprtdiag_psr/sparc/schumacher/common/schumacher.c */
static uint64_t
picl_get_uint_propval(picl_nodehdl_t modh, char *prop_name, int *ret)
{
	int		err;
	picl_prophdl_t	proph;
	picl_propinfo_t	pinfo;
	uint8_t		uint8v;
	uint16_t	uint16v;
	uint32_t	uint32v;
	uint64_t	uint64v;

	hi_cpu_dprintf("picl_get_uint_propval: reading property %s\n", prop_name);

	err = picl_get_propinfo_by_name(modh, prop_name, &pinfo, &proph);
	if (err != PICL_SUCCESS) {
		*ret = err;
		return (0);
	}

	hi_cpu_dprintf("picl_get_uint_propval: type: %d size: %d\n", pinfo.type, pinfo.size);

	if ((pinfo.type != PICL_PTYPE_INT) &&
	    (pinfo.type != PICL_PTYPE_UNSIGNED_INT)) {
		*ret = PICL_FAILURE;
		return (0);
	}

	switch (pinfo.size) {
	case sizeof (uint8_t):
		err = picl_get_propval(proph, &uint8v, sizeof (uint8v));
		*ret = err;
		return (uint8v);
	case sizeof (uint16_t):
		err = picl_get_propval(proph, &uint16v, sizeof (uint16v));
		*ret = err;
		return (uint16v);
	case sizeof (uint32_t):
		err = picl_get_propval(proph, &uint32v, sizeof (uint32v));
		*ret = err;
		return (uint32v);
	case sizeof (uint64_t):
		err = picl_get_propval(proph, &uint64v, sizeof (uint64v));
		*ret = err;
		return (uint64v);
	default:	/* not supported size */
		*ret = PICL_FAILURE;
		return (0);
	}
}

static
hi_cpu_object_t* hi_cpu_create_cache(hi_cpu_object_t* parent, struct hi_cpu_sol_cache* caches, int cid) {
	int cacheid = HI_CPU_CACHEID(caches[cid].cache.c_level, caches[cid].cache.c_type);
	hi_cpu_object_t* cache = hi_cpu_object_create(parent, HI_CPU_CACHE, cacheid);

	memcpy(&cache->cache, &caches[cid].cache, sizeof(hi_cpu_cache_t));

	hi_cpu_object_add(cache);

	return cache;
}

int hi_cpu_walk_cache(picl_nodehdl_t hdl, void *args) {
	struct cache_walk_args* cache_args = (struct cache_walk_args*) args;
	uint64_t cpuid;
	int ret;

	/* Local copy of solaris caches */
	struct hi_cpu_sol_cache* caches = NULL;
	struct hi_cpu_sol_cache* last_cache = NULL;
	int c_count = sizeof(solaris_caches) / sizeof(struct hi_cpu_sol_cache);
	int cid, last_cid;
	char c_propname[48];

	hi_cpu_object_t* core;
	hi_cpu_object_t* strand;
	hi_cpu_object_t* cache;

	cpuid = picl_get_uint_propval(hdl, "ID", &ret);
	if(ret != PICL_SUCCESS)
		return PICL_WALK_CONTINUE;

	if(cpuid != cache_args->cpu) {
		return PICL_WALK_CONTINUE;
	}

	/* Found PICL object related to CPU */
	hi_cpu_dprintf("hi_cpu_walk_cache: processing processor #%d shrlc: %d 1stcpu: %d\n", (int) cpuid,
					cache_args->shared_last_cache, cache_args->first_cpu);

	strand = hi_cpu_find_byid(cache_args->chip, HI_CPU_STRAND, cache_args->cpu);
	if(!strand)
		return PICL_WALK_TERMINATE;

	core = HI_CPU_PARENT(strand);

	caches = mp_malloc(sizeof(solaris_caches));
	memcpy(caches, solaris_caches, sizeof(solaris_caches));

	/* FIXME: On OPL caches are located on core object, not on CPU */
	for(cid = 0; cid < c_count; ++cid) {
		snprintf(c_propname, 48, "%s-size", caches[cid].prefix);
		caches[cid].cache.c_size = picl_get_uint_propval(hdl, c_propname, &ret);
		if(ret != PICL_SUCCESS)
			continue;

		snprintf(c_propname, 48, "%s-associativity", caches[cid].prefix);
		caches[cid].cache.c_associativity = picl_get_uint_propval(hdl, c_propname, &ret);
		if(ret != PICL_SUCCESS)
			continue;

		if(caches[cid].cache.c_level != HI_CPU_CACHE_TLB_LEVEL) {
			snprintf(c_propname, 48, "%s-line-size", caches[cid].prefix);
			caches[cid].cache.c_unit_size.line = picl_get_uint_propval(hdl, c_propname, &ret);
			if(ret != PICL_SUCCESS)
				continue;
		}

		hi_cpu_dprintf("hi_cpu_walk_cache: found cache %s\n", caches[cid].prefix);
		caches[cid].found = B_TRUE;
		last_cid = cid;
	}

	/* FIXME: for SPARC it's itlb-entries and dtlb-entries, need to add them too */

	if(cache_args->shared_last_cache) {
		if(cache_args->first_cpu)
			hi_cpu_create_cache(cache_args->chip, caches, last_cid);
		--last_cid;
	}

	for(cid = 0; cid <= last_cid; ++cid) {
		if(caches[cid].found) {
			hi_cpu_create_cache(core, caches, cid);
		}
	}

	mp_free(caches);

	return PICL_WALK_TERMINATE;
}

void hi_cpu_proc_cache(hi_cpu_object_t* chip, processorid_t cpu, boolean_t shared_last_cache, boolean_t first_cpu) {
	picl_nodehdl_t nodehdl;
	struct cache_walk_args c_args;

	if(picl_get_root(&nodehdl) != PICL_SUCCESS)
		return;

	c_args.chip = chip;
	c_args.cpu = cpu;
	c_args.shared_last_cache = shared_last_cache;
	c_args.first_cpu = first_cpu;

	/* FIXME: on OPL caches are located in "core" objects */
	picl_walk_tree_by_class(nodehdl, "cpu", &c_args, hi_cpu_walk_cache);
}

static
void cpu_kstat_read_freq(hi_cpu_object_t* chip, kstat_t *ksp) {
	kstat_named_t	*knp;

	if ((knp = kstat_data_lookup(ksp, "clock_MHz")) == NULL)
		return;

	chip->chip.cp_freq = knp->value.l;
}

static
void cpu_kstat_read_model(hi_cpu_object_t* chip, kstat_t *ksp) {
	kstat_named_t	*knp;

	if ((knp = kstat_data_lookup(ksp, "brand")) == NULL)
		return;

	hi_cpu_set_chip_name(chip, knp->value.str.addr.ptr);
}

hi_cpu_object_t* hi_cpu_proc_cpu(hi_cpu_object_t* node, processorid_t cpu, long* cacheid) {
	kstat_ctl_t		*kc;
	kstat_t			*ksp;
	kstat_named_t	*knp;

	long coreid, chipid;

	hi_cpu_object_t* chip;
	hi_cpu_object_t* core;
	hi_cpu_object_t* strand;

	if((kc = kstat_open()) == NULL)
		return NULL;

	if((ksp = kstat_lookup(kc, "cpu_info", (int) cpu, NULL)) == NULL) {
		kstat_close(kc);
		return NULL;
	}

	if(kstat_read(kc, ksp, NULL) == -1) {
		kstat_close(kc);
		return NULL;
	}

	/* Read CPU information from kstat */
	if ((knp = kstat_data_lookup(ksp, "chip_id")) == NULL) {
		kstat_close(kc);
		return NULL;
	}
	chipid = knp->value.l;

	if ((knp = kstat_data_lookup(ksp, "core_id")) == NULL) {
		kstat_close(kc);
		return NULL;
	}
	coreid = knp->value.l;

	if ((knp = kstat_data_lookup(ksp, "cache_id")) != NULL) {
		*cacheid = knp->value.l;
	}

	/* Create processor/core objects if necessary */
	chip = hi_cpu_find_byid(node, HI_CPU_CHIP, chipid);
	if(chip == NULL) {
		chip = hi_cpu_object_create(node, HI_CPU_CHIP, chipid);

		cpu_kstat_read_freq(chip, ksp);
		cpu_kstat_read_model(chip, ksp);

		hi_cpu_object_add(chip);

		core = NULL;
	}
	else {
		core = hi_cpu_find_byid(node, HI_CPU_CORE, coreid);
	}

	if(core == NULL) {
		core = hi_cpu_object_create(chip, HI_CPU_CORE, coreid);
		hi_cpu_object_add(core);
	}

	strand = hi_cpu_object_create(core, HI_CPU_STRAND, (int) cpu);
	hi_cpu_object_add(strand);

	hi_cpu_dprintf("hi_cpu_proc_cpu: processing processor #%d freq: %lld model: %s\n", (int) cpu,
						(long long) chip->chip.cp_freq, chip->chip.cp_name);

	kstat_close(kc);

	return chip;
}

void hi_cpu_proc_lgrp(lgrp_cookie_t cookie, lgrp_id_t lgrp) {
	hi_cpu_object_t* node = NULL;
	lgrp_id_t children[LGRP_MAX_CHILDREN];
	processorid_t cpuids[LGRP_MAX_CPUS];
	hi_cpu_object_t* chips[LGRP_MAX_CPUS] = { NULL };
	long cacheids[LGRP_MAX_CPUS] = {-1};

	int nchildren;
	int ncpus;
	int i;

	boolean_t shared_last_cache = B_FALSE;

	node = hi_cpu_object_create(NULL, HI_CPU_NODE, (int) lgrp);
	hi_cpu_object_add(node);

	node->node.cm_mem_total = lgrp_mem_size(cookie, lgrp, LGRP_MEM_SZ_INSTALLED, LGRP_CONTENT_DIRECT);
	node->node.cm_mem_free  = lgrp_mem_size(cookie, lgrp, LGRP_MEM_SZ_FREE, LGRP_CONTENT_DIRECT);

	hi_cpu_dprintf("hi_cpu_lgrp_walk: processing lgrp #%d\n", (int) lgrp);

	ncpus = lgrp_cpus(cookie, lgrp, cpuids, LGRP_MAX_CPUS, LGRP_CONTENT_DIRECT);

	/* First pass: walking cpus and determining if last cache is shared
	 * NOTE: On SPARC cache_id kstat is not provided, so HI will assume
	 * that all cache is private to core. */
	for(i = 0; i < ncpus; ++i) {
		chips[i] = hi_cpu_proc_cpu(node, cpuids[i], &cacheids[i]);

		if(i > 0 && cacheids[i] != -1 && (cacheids[i-1] == cacheids[i]))
			shared_last_cache = B_TRUE;
	}

	if(shared_last_cache)
		hi_cpu_dprintf("hi_cpu_lgrp_walk: lgrp #%d => shared_last_cache\n", (int) lgrp);

	/* Second pass: walking caches. Process last level of cache only for
	 * first CPU or then no shared_last_cache found */
	for(i = 0; i < ncpus; ++i) {
		if(chips[i] != NULL)
			hi_cpu_proc_cache(chips[i], cpuids[i], shared_last_cache, i == 0);
	}

	nchildren = lgrp_children(cookie, lgrp, children, LGRP_MAX_CHILDREN);
	for(i = 0; i < nchildren; ++i) {
		hi_cpu_proc_lgrp(cookie, children[i]);
	}
}

PLATAPI int hi_cpu_probe(void) {
	lgrp_cookie_t cookie;
	lgrp_id_t root;

	if(picl_initialize() != PICL_SUCCESS)
		return HI_PROBE_ERROR;

	cookie = lgrp_init(LGRP_VIEW_OS);
	root = lgrp_root(cookie);

	if(root == -1) {
		picl_shutdown();
		return HI_PROBE_ERROR;
	}

	hi_cpu_proc_lgrp(cookie, root);

	lgrp_fini(cookie);
	picl_shutdown();

	return HI_PROBE_OK;
}

