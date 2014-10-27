
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



#include <cpuinfo.h>
#include <pathutil.h>
#include <plat/sysfs.h>
#include <ilog2.h>
#include <mempool.h>
#include <autostring.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

/**
 * cpuinfo (Linux)
 *
 * Reads information about cpus (that are considered as NUMA-nodes)
 * from /sys/devices/system/node.
 */

/* FIXME: Should be taken from kernel config */
#define HI_LINUX_MAXCPUS		256
#define HI_LINUX_CPUMASK_LEN	4

#define SYS_NODE_PATH 	"/sys/devices/system/node"
#define SYS_CPU_PATH 	"/sys/devices/system/cpu"
#define PROC_CPUINFO_PATH 	"/proc/cpuinfo"

typedef uint32_t hi_linux_cpumask_t[HI_LINUX_CPUMASK_LEN];

/* Returns most significant cpu for mask */
static
int hi_linux_cpumask_msc(hi_linux_cpumask_t mask) {
	int i;
	int msc = -1, tmp;

	for(i = 0; i < HI_LINUX_CPUMASK_LEN; ++i) {
		if(mask[i] != 0) {
			msc = __msb32(mask[i]) * ((i == 0)? 1 : i * 32);
		}
	}

	return msc;
}

/* Needed to check whether */
static
void hi_linux_cpumask_set(hi_linux_cpumask_t mask, int strandid) {
	int i;

	if(strandid > HI_LINUX_MAXCPUS)
		return;

	mask[strandid / 32] |= (1 << (strandid % 32));
}

static
boolean_t hi_linux_cpumask_eq(hi_linux_cpumask_t mask1, hi_linux_cpumask_t mask2) {
	int i;

	for(i = 0; i < HI_LINUX_CPUMASK_LEN; ++i) {
		if(mask1[i] != mask2[i])
			return B_FALSE;
	}

	return B_TRUE;
}

static
void hi_linux_cpumask_create(hi_cpu_object_t* parent, hi_linux_cpumask_t core_mask) {
	hi_object_child_t* child;
	hi_cpu_object_t* cpu_object;

	hi_for_each_child(child, &parent->hdr) {
		cpu_object = HI_CPU_FROM_OBJ(child->object);

		if(cpu_object->type == HI_CPU_STRAND) {
			hi_linux_cpumask_set(core_mask, cpu_object->id);
		}
	}
}

hi_cpu_cache_type_t hi_linux_get_cache_type(const char* cache_name) {
	char type_str[32] = "";

	hi_linux_sysfs_readstr(SYS_NODE_PATH, cache_name, "type", type_str, 32);
	hi_linux_sysfs_fixstr(type_str);

	if(strcmp(type_str, "Unified ") == 0) {
		return HI_CPU_CACHE_UNIFIED;
	}
	else if(strcmp(type_str, "Data ") == 0) {
		return HI_CPU_CACHE_DATA;
	}
	else if(strcmp(type_str, "Instruction ") == 0) {
		return HI_CPU_CACHE_INSTRUCTION;
	}

	return HI_CPU_CACHE_UNIFIED;
}

void hi_linux_proc_cache(const char* name, void* arg) {
	hi_cpu_object_t* strand = (hi_cpu_object_t*) arg;

	hi_cpu_object_t* core = HI_CPU_PARENT(strand);
	hi_cpu_object_t* chip = HI_CPU_PARENT(core);
	hi_cpu_object_t* node = HI_CPU_PARENT(chip);

	hi_cpu_object_t* parent;
	hi_cpu_object_t* cache;

	char cache_name[64];

	hi_cpu_cache_type_t cache_type;

	int level, associativity, line_size;
	uint32_t size;

	hi_linux_cpumask_t share_mask = { 0 };
	hi_linux_cpumask_t core_mask = { 0 };
	int share_msc;

	snprintf(cache_name, 64, "node%d/cpu%d/cache/%s", node->id, strand->id, name);

	hi_linux_sysfs_readbitmap(SYS_NODE_PATH, cache_name, "shared_cpu_list", share_mask, HI_LINUX_CPUMASK_LEN);

	/* Linux duplicates entries for shared caches
	 * Process only entries for one cpu by selecting most significant bit */
	share_msc = hi_linux_cpumask_msc(share_mask);
	hi_cpu_dprintf("hi_linux_proc_cache: found cache %s, share_msc: %d strandid: %d\n", cache_name, share_msc, strand->id);

	if(strand->id != share_msc)
		return;

	/* Check if cache is core-level or node-level.
	 * Walk over core strands and check if their mask matches share_mask*/
	hi_linux_cpumask_create(core, core_mask);
	parent = (hi_linux_cpumask_eq(share_mask, core_mask))? core : chip;

	/* Read cache information */
	level = (int) hi_linux_sysfs_readuint(SYS_NODE_PATH, cache_name, "level", 8);
	associativity = (int) hi_linux_sysfs_readuint(SYS_NODE_PATH, cache_name, "ways_of_associativity", 0);
	line_size = (int) hi_linux_sysfs_readuint(SYS_NODE_PATH, cache_name, "coherency_line_size", 0);
	cache_type = hi_linux_get_cache_type(cache_name);
	/* XXX: Does Linux always provides cache size in kb? */
	size = (uint32_t) hi_linux_sysfs_readuint(SYS_NODE_PATH, cache_name, "size", 8) * SZ_KB;

	cache = hi_cpu_object_create(parent, HI_CPU_CACHE, HI_CPU_CACHEID(level, cache_type));
	cache->cache.c_level = level;
	cache->cache.c_size = size;
	cache->cache.c_type = cache_type;
	cache->cache.c_associativity = associativity;
	cache->cache.c_unit_size.line = line_size;

	hi_cpu_dprintf("hi_linux_proc_cache: => level: %d type: %d size: %ld\n", level, cache_type, size);

	hi_cpu_object_add(cache);
}

static uint64_t meminfo_read_param(const char* meminfo, const char* tag) {
	char* p;
	uint64_t mem;

	p = strstr(meminfo, tag);
	if(!p)
		return 0;

	p = strpbrk(p, "1234567890");
	if(!p)
		return 0;

	/* XXX: Does Linux always provides memory size in kb? */
	mem = strtoull(p, NULL, 10) * SZ_KB;

	hi_cpu_dprintf("hi_linux_read_meminfo: tag: %s data: %lld\n", tag, tag, mem);

	return mem;
}

void hi_linux_get_meminfo(hi_cpu_object_t* node, const char* name) {
	char* meminfo = mp_malloc(4096);

	hi_linux_sysfs_readstr(SYS_NODE_PATH, name, "meminfo", meminfo, 4096);

	node->node.cm_mem_total = meminfo_read_param(meminfo, "MemTotal:");
	node->node.cm_mem_free = meminfo_read_param(meminfo, "MemFree:");

	mp_free(meminfo);
}

static boolean_t cpuinfo_read_var(char* line, const char* param, char** value) {
	size_t paramlen = strlen(param);
	size_t linelen = strlen(line);
	const char* p = line + paramlen;
	char* end;
	int len = 0;

	int mode = 0;

	/* Cut out last '\n' */
	end = line + linelen - 1;
	if(*end == '\n')
		*end = '\0';

	if(strncmp(line, param, paramlen) == 0) {
		/* Ignore parameter name with whitespaces. Acts as FSM.  */
		while((mode == 0 && (*p != ':')) ||
			  (mode == 1 && isspace(*p) ) ) {
			if(p > end)
				return B_FALSE;
			if(*p == ':')
				mode = 1;

			++p;
		}

		aas_copy(value, p + 1);

		return B_TRUE;
	}

	return B_FALSE;
}

hi_cpu_object_t* hi_linux_proc_chip(hi_cpu_object_t* node, int strandid, int chipid) {
	hi_cpu_object_t* chip;
	FILE* cpuinfo = fopen(PROC_CPUINFO_PATH, "r");
	uint32_t cpufreq = 0;
	char cpu_name[32];

	char line[256];
	char* value;

	long ci_chipid;

	boolean_t freq_read = B_FALSE, model_read = B_FALSE;

	chip = hi_cpu_object_create(node, HI_CPU_CHIP, chipid);

	/* Try to read maximum frequency from cpufreq driver
	 * If failed, go back to /proc/cpuinfo information */
	snprintf(cpu_name, 32, "cpu%d", strandid);
	cpufreq = hi_linux_sysfs_readuint(SYS_CPU_PATH, cpu_name, "cpufreq/cpuinfo_max_freq", 0);

	if(cpufreq != 0) {
		cpufreq = cpufreq / 1000;
		freq_read = B_TRUE;
	}

	/* Read modelname from /proc/cpuinfo */
	if(cpuinfo) {
		while(fgets(line, 256, cpuinfo) != NULL) {
			aas_init(&value);

			if(cpuinfo_read_var(line, "processor", &value)) {
				ci_chipid = strtol(value, NULL, 10);
				aas_free(&value);

				continue;
			}

			if(ci_chipid == chipid) {
				if(!freq_read && cpuinfo_read_var(line, "cpu MHz", &value)) {
					/* Frequency in MHz */
					cpufreq = strtoul(value, NULL, 10);
					freq_read = B_TRUE;
				}
				else if(cpuinfo_read_var(line, "model name", &value)) {
					hi_cpu_set_chip_name(chip, value);
					model_read = B_TRUE;
				}
			}

			aas_free(&value);

			if(freq_read && model_read)
				break;
		}
	}

	chip->chip.cp_freq = cpufreq;

	hi_cpu_object_add(chip);

	return chip;
}

void hi_linux_proc_strand(int strandid, void* arg) {
	hi_cpu_object_t* node = (hi_cpu_object_t*) arg;
	hi_cpu_object_t* core;
	hi_cpu_object_t* chip;
	hi_cpu_object_t* strand;

	char name[32];
	int coreid, chipid;

	char cache_path[256];

	snprintf(name, 32, "node%d/cpu%d", node->id, strandid);

	coreid = hi_linux_sysfs_readuint(SYS_NODE_PATH, name, "topology/core_id", HI_LINUX_MAXCPUS);
	chipid = hi_linux_sysfs_readuint(SYS_NODE_PATH, name, "topology/physical_package_id", HI_LINUX_MAXCPUS);

	if(coreid == HI_LINUX_MAXCPUS || chipid == HI_LINUX_MAXCPUS)
		return HI_LINUX_SYSFS_ERROR;

	/* Find processor and core objects. If they do not exist, create new ones */
	chip = hi_cpu_find_byid(node, HI_CPU_CHIP, chipid);
	if(chip == NULL) {
		chip = hi_linux_proc_chip(node, strandid, chipid);
		core = NULL;
	}
	else {
		core = hi_cpu_find_byid(chip, HI_CPU_CORE, coreid);
	}

	if(core == NULL) {
		core = hi_cpu_object_create(chip, HI_CPU_CORE, coreid);
		hi_cpu_object_add(core);
	}

	/* Create strand object */
	strand = hi_cpu_object_create(core, HI_CPU_STRAND, strandid);
	hi_cpu_object_add(strand);

	path_join(cache_path, 256, SYS_NODE_PATH, name, "cache", NULL);

	hi_cpu_dprintf("hi_linux_proc_strand: strandid: %d coreid: %d\n", strandid, coreid);

	hi_linux_sysfs_walk(cache_path, hi_linux_proc_cache, strand);
}

void hi_linux_proc_cpu(int nodeid, void* arg) {
	char name[32];
	hi_cpu_object_t* node;

	snprintf(name, 32, "node%d", nodeid);

	node = hi_cpu_object_create(NULL, HI_CPU_NODE, nodeid);
	hi_cpu_object_add(node);

	hi_cpu_dprintf("hi_linux_proc_cpu: nodeid: %d\n", nodeid);

	hi_linux_sysfs_walkbitmap(SYS_NODE_PATH, name, "cpulist", HI_LINUX_MAXCPUS, hi_linux_proc_strand, node);

	hi_linux_get_meminfo(node, name);
}

PLATAPI int hi_cpu_probe(void) {
	hi_linux_sysfs_walkbitmap(SYS_NODE_PATH, "", "online", HI_LINUX_MAXCPUS, hi_linux_proc_cpu, NULL);

	return HI_PROBE_OK;
}

