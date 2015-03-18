
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



#ifndef SYSFS_H_
#define SYSFS_H_

#include <tsload/defs.h>

/**
 * @module sysfs
 * 
 * As Linux Kernel documentation implies, sysfs is:
 * 
 *  >  sysfs is a ram-based filesystem initially based on ramfs. It provides
 * a means to export kernel data structures, their attributes, and the 
 * linkages between them to userspace. 
 * 
 * Currently, sysfs contains many information vital for HostInfo: i.e. NUMA
 * topology and properties of block devices.
 * 
 * Because each attribute represented as a single file, accessing it
 * require following actions:
 * 	1. Build path of sysfs file (usually done by `path_join_aas)`. 
 *  2. Open that file and check for errors
 *  3. Read data and coerce types where needed
 * Following functions can do that in a single call.
 * 
 * #### Paths
 * 
 * Path is usually passed to these functions in three arguments:
 *  * `root` -- root directory where objects that are currently processed \
 *              are located
 *  * `name` -- name of processed object
 *  * `object` -- name or path of attribute
 * 
 * These arguments are then passed to `path_join*` functions as varargs, so 
 * If you set one of them to NULL, following arguments will be omitted, 
 * so if you do not have `name`, you should pass attribute name as `name`
 * and `NULL` as `object`.
 * 
 * I.e. to get UUID of DM device, `/sys/block`, `dm-0` and `dm/uuid` are passed.
 * It is useful because, `/sys/block` is global for all devices, and `dm/uuid` is
 * a universal path to UUID, while `dm-0` may be easily replaced with other DM names.
 */

#define HI_LINUX_SYSFS_BLOCK	32

#define HI_LINUX_SYSFS_OK		0
#define HI_LINUX_SYSFS_ERROR 	-1

int hi_linux_sysfs_readstr(const char* root, const char* name, const char* object,
						   char* str, int len);
int hi_linux_sysfs_readstr_aas(const char* root, const char* name, const char* object,
						   	   char** aas);
uint64_t hi_linux_sysfs_readuint(const char* root, const char* name, const char* object,
						   uint64_t defval);
int hi_linux_sysfs_readbitmap(const char* root, const char* name, const char* object,
						   uint32_t* bitmap, int len);
void hi_linux_sysfs_fixstr(char* p);
void hi_linux_sysfs_fixstr2(char* p);

int hi_linux_sysfs_walk(const char* root,
					   void (*proc)(const char* name, void* arg), void* arg);
int hi_linux_sysfs_walkbitmap(const char* root, const char* name, const char* object, int count,
					   	      void (*proc)(int id, void* arg), void* arg);
int hi_linux_sysfs_readlink_aas(const char* root, const char* name, const char* object,
						   	    char** aas, boolean_t basename);

#include <hitrace.h>

#endif /* SYSFS_H_ */

