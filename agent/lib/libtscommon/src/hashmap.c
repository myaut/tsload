
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



#include <hashmap.h>
#include <mempool.h>
#include <threads.h>
#include <autostring.h>

#include <assert.h>

/* Helper routines for hash map */
STATIC_INLINE hm_item_t* hm_next(hashmap_t* hm, hm_item_t* obj) {
	return *((hm_item_t**) (obj + hm->hm_off_next));
}

STATIC_INLINE void hm_set_next(hashmap_t* hm, hm_item_t* obj, hm_item_t* next) {
	*((hm_item_t**) (obj + hm->hm_off_next)) = next;
}

STATIC_INLINE hm_key_t* hm_get_key(hashmap_t* hm, hm_item_t* obj) {
	hm_key_t* key = (obj + hm->hm_off_key);

	if(hm->hm_indirect) {
		key = * (void**) key;
	}

	return key;
}

STATIC_INLINE unsigned hm_hash_object(hashmap_t* hm, hm_item_t* obj) {
	return hm->hm_hash_key(hm_get_key(hm, obj));
}

/**
 * Initialize static hash map.
 *
 * Because hash map usually a global object, it doesn't
 * support formatted names
 *
 * @param hm hash map to initialize
 * @param name identifier for hash map
 * */
void hash_map_init(hashmap_t* hm, const char* name) {
	int i = 0;

	for(i = 0; i < hm->hm_size; ++i) {
		hm->hm_heads[i] = NULL;
	}

	mutex_init(&hm->hm_mutex, "hm-%s", name);

	if(name != NULL)
		aas_copy(&hm->hm_name, name);
}

/**
 * Create dynamic hashmap
 *
 * @param base static hashmap, that contains compare/hash functions, size and offsets
 * @param namefmt name format
 */
hashmap_t* hash_map_create(hashmap_t* base, const char* namefmt, ...) {
	hashmap_t* hm = mp_malloc(sizeof(hashmap_t));
	va_list va;

	hm->hm_type = HASH_MAP_DYNAMIC;

	hm->hm_indirect = base->hm_indirect;

	hm->hm_size = base->hm_size;
	hm->hm_heads = mp_malloc(base->hm_size * sizeof(hm_item_t*));

	hm->hm_off_key = base->hm_off_key;
	hm->hm_off_next = base->hm_off_next;

	hm->hm_compare = base->hm_compare;
	hm->hm_hash_key = base->hm_hash_key;

	hash_map_init(hm, NULL);

	va_start(va, namefmt);
	aas_vprintf(&hm->hm_name, namefmt, va);
	va_end(va);

	return hm;
}

/**
 * Destroy hash map
 *
 * It shouldn't contain objects (or assertion will rise)
 */
void hash_map_destroy(hashmap_t* hm) {
	int i = 0;

	for(i = 0; i < hm->hm_size; ++i) {
		assert(hm->hm_heads[i] == NULL);
	}

	aas_free(&hm->hm_name);

	mutex_destroy(&hm->hm_mutex);

	if(hm->hm_type == HASH_MAP_DYNAMIC) {
		mp_free(hm->hm_heads);
		mp_free(hm);
	}
}

/**
 * Insert element into hash map
 *
 * @param hm hash map
 * @param object object to be inserted
 *
 * @return HASH_MAP_OK if object was successfully inserted or HASH_MAP_DUPLICATE if \
 * object with same key (not hash!) already exists in hash map
 * */
int hash_map_insert(hashmap_t* hm, hm_item_t* object) {
	unsigned hash = hm_hash_object(hm, object);
	int ret = HASH_MAP_OK;

	hm_item_t** head = hm->hm_heads + hash;
	hm_item_t* iter;
	hm_item_t* next;

	mutex_lock(&hm->hm_mutex);
	if(*head == NULL) {
		*head = object;
	}
	else {
		iter = *head;
		next = hm_next(hm, iter);

		/* Walk through chain until reach tail */
		while(B_TRUE) {
			if(hm->hm_compare(hm_get_key(hm, iter),
			                  hm_get_key(hm, object))) {
				ret = HASH_MAP_DUPLICATE;
				goto done;
			}

			/* Found tail */
			if(next == NULL)
				break;

			iter = next;
			next = hm_next(hm, iter);
		}

		/* Insert object into tail */
		hm_set_next(hm, iter, object);
	}

done:
	mutex_unlock(&hm->hm_mutex);

	return ret;
}

/**
 * Remove element from hash map
 *
 * @param hm hash map
 * @param object object to be removed
 *
 * @return HASH_MAP_OK if object was successfully remove or HASH_MAP_NOT_FOUND if \
 * object is not
 * */
int hash_map_remove(hashmap_t* hm, hm_item_t* object) {
	unsigned hash = hm_hash_object(hm, object);
	int ret = HASH_MAP_OK;

	hm_item_t** head = hm->hm_heads + hash;
	hm_item_t* iter;
	hm_item_t* next;

	mutex_lock(&hm->hm_mutex);

	if(*head == NULL) {
		return HASH_MAP_NOT_FOUND;
	}

	if(*head == object) {
		iter = hm_next(hm, *head);
		*head = iter;
	}
	else {
		iter = *head;
		next = hm_next(hm, iter);

		/* Find object that is previous for
		 * object we are removing and save it to iter */
		while(next != object) {
			if(next == NULL) {
				ret = HASH_MAP_NOT_FOUND;
				goto done;
			}

			iter = next;
			next = hm_next(hm, iter);
		}

		/* Remove object from chain */
		hm_set_next(hm, iter, hm_next(hm, object));
	}

done:
	mutex_unlock(&hm->hm_mutex);
	return ret;
}

/**
 * Find object in hash map by key without locking hashmap
 * call it only from walker context!
 */
void* hash_map_find_nolock(hashmap_t* hm, const hm_key_t* key) {
	unsigned hash = hm->hm_hash_key(key);
	hm_item_t* iter;

	iter = hm->hm_heads[hash];

	while(iter != NULL) {
		if(hm->hm_compare(hm_get_key(hm, iter), key))
			break;

		iter =  hm_next(hm, iter);
	}

	return iter;
}

/**
 * Find object in hash map by key
 */
void* hash_map_find(hashmap_t* hm, const hm_key_t* key) {
	hm_item_t* iter;

	mutex_lock(&hm->hm_mutex);
	iter = hash_map_find_nolock(hm, key);
	mutex_unlock(&hm->hm_mutex);

	return iter;
}

/**
 * Walk over objects in hash map and execute func for each object.
 *
 * Func proto: int (*func)(void* object, void* arg)
 * Function func may return HM_WALKER_CONTINUE or HM_WALKER_STOP.
 *
 * For HM_WALKER_STOP, hash_map_walk will stop walking over hash map and return
 * current object
 *
 * @note because of nature of hash maps, walking order is undefined
 *
 * @param hm hash map
 * @param func function that will be called for each object
 * @param arg argument that will be passed as second argument for func
 *
 * @return NULL or object where func returned STOP
 */
void* hash_map_walk(hashmap_t* hm, hm_walker_func func, void* arg) {
	int i = 0;
	int ret = 0;
	hm_item_t* iter = NULL;
	hm_item_t* next = NULL;
	hm_item_t* prev = NULL;

	mutex_lock(&hm->hm_mutex);
	for(i = 0; i < hm->hm_size; ++i) {
		prev = NULL;
		next = NULL;
		iter = hm->hm_heads[i];

		while(iter != NULL) {
			next = hm_next(hm, iter);

			ret = func(iter, arg);

			if(ret & HM_WALKER_REMOVE) {
				if(prev == NULL) {
					/* Entire chain before iter was removed, so this is head */
					hm->hm_heads[i] = next;
				}
				else {
					hm_set_next(hm, prev, next);
				}
			}
			else {
				/* Save prev pointer */
				prev = iter;
			}

			if(ret & HM_WALKER_STOP)
				goto done;

			iter = next;
		}

		if(prev == NULL) {
			/* Entire chain was removed */
			hm->hm_heads[i] = NULL;
		}
	}

	iter = NULL;

done:
	mutex_unlock(&hm->hm_mutex);

	return iter;
}

/**
 * Calculate hash for strings */
unsigned hm_string_hash(const hm_key_t* key, unsigned mask) {
	const char* p = (const char*) key;
	unsigned hash = 0;

	while(*p != 0)
		hash += *p++;

	return hash & mask;
}
