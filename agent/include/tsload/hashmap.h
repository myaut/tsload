
/*
    This file is part of TSLoad.
    Copyright 2012-2014, Sergey Klyaus, ITMO University

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



#ifndef HASHMAP_H_
#define HASHMAP_H_

#include <tsload/defs.h>

#include <tsload/threads.h>

#include <stddef.h>
#include <string.h>


/**
 * @module Hash maps
 * */

#ifdef _MSC_VER
typedef char hm_item_t;
typedef char hm_key_t;
#else
typedef void hm_item_t;
typedef void hm_key_t;
#endif

#define HASH_MAP_STATIC		1
#define HASH_MAP_DYNAMIC	2

typedef struct {
	size_t			hm_size;
	hm_item_t** 	hm_heads;

	boolean_t		hm_indirect;
	int 			hm_type;

	thread_mutex_t hm_mutex;

	ptrdiff_t hm_off_key;
	ptrdiff_t hm_off_next;

	char* hm_name;

	unsigned (*hm_hash_key)(const hm_key_t* key);
	boolean_t (*hm_compare)(const hm_key_t* key1, const hm_key_t* key2);
} hashmap_t;

typedef int (*hm_walker_func)(hm_item_t* object, void* arg);

LIBEXPORT void hash_map_init(hashmap_t* hm, const char* name);
LIBEXPORT hashmap_t* hash_map_create(hashmap_t* base, const char* namefmt, ...);
LIBEXPORT void hash_map_destroy(hashmap_t* hm);

LIBEXPORT int  hash_map_insert(hashmap_t* hm, hm_item_t* object);
LIBEXPORT int  hash_map_remove(hashmap_t* hm, hm_item_t* object);
LIBEXPORT void* hash_map_find(hashmap_t* hm, const hm_key_t* key);
LIBEXPORT void* hash_map_find_nolock(hashmap_t* hm, const hm_key_t* key);
LIBEXPORT void* hash_map_walk(hashmap_t* hm, hm_walker_func func, void* arg);

LIBEXPORT unsigned hm_string_hash(const hm_key_t* str, unsigned mask);

/**
 * Declare hash map
 *
 * @param name name of hash map var (all corresponding objects will be named hm_(object)_(name))
 * @param is_indirect if set to B_TRUE, key_field is considered a pointer to a key, not a key itself
 * @param type type of elements
 * @param size size of hash map (not of type)
 * @param key_field field of element that contains key
 * @param next_field field of element that contains pointer to next object
 * @param hm_hash_body function body {} that hashes key and returns hash (params: const void* key)
 * @param hm_compare_body function body {} that compares to keys (params: const void* key1, const void* key2)
 */
#define DECLARE_HASH_MAP(name, is_indirect, type, size, key_field, next_field, hm_hash_body, hm_compare_body) \
	static boolean_t								\
	hm_compare_##name(const hm_key_t* key1, 		\
					  const hm_key_t* key2) 		\
	hm_compare_body								    \
												    \
	static unsigned									\
	hm_hash_##name(const hm_key_t* key)				\
	hm_hash_body									\
													\
	static type* hm_heads_##name[size];				\
	hashmap_t name = {								\
		SM_INIT(.hm_size, size),							\
		SM_INIT(.hm_heads, (void**) hm_heads_##name),		\
		SM_INIT(.hm_indirect, is_indirect),					\
		SM_INIT(.hm_type, HASH_MAP_STATIC),					\
		SM_INIT(.hm_mutex, THREAD_MUTEX_INITIALIZER),		\
		SM_INIT(.hm_off_key, offsetof(type, key_field)),	\
		SM_INIT(.hm_off_next, offsetof(type, next_field)),	\
		SM_INIT(.hm_name, #name ),							\
		SM_INIT(.hm_hash_key, hm_hash_##name),				\
		SM_INIT(.hm_compare, hm_compare_##name)				\
	};

/**
 * Same as DECLARE_HASH_MAP, but assumes that key field is string declared as char* (AAS)
 */
#define DECLARE_HASH_MAP_STRKEY(name, type, size, key_field, next_field, mask)	\
	DECLARE_HASH_MAP(name, B_TRUE, type, size, key_field, next_field, 			\
		{ return hm_string_hash(key, mask); },									\
		{ return strcmp((char*) key1, (char*) key2) == 0; }						\
	)



/**
 * hm_* functions return codes
 *
 * @value HASH_MAP_OK everything went fine
 * @value HASH_MAP_DUPLICATE element with such key exists
 * @value HASH_MAP_NOT_FOUND such element not found
 * */
#define HASH_MAP_OK				0
#define HASH_MAP_DUPLICATE		-1
#define HASH_MAP_NOT_FOUND		-2

/**
 * Walker flags for hash_map_walk. Returned by walker func.
 *
 * @value HM_WALKER_CONTINUE continue walking
 * @value HM_WALKER_STOP stop walking here and return current object (useful for "find")
 * @value HM_WALKER_REMOVE remove current element
 * */
#define HM_WALKER_CONTINUE		0
#define HM_WALKER_STOP			0x01
#define HM_WALKER_REMOVE		0x02

#endif

