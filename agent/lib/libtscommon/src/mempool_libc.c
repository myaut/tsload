/*
 * mempool_libc.c
 *
 *  Created on: 05.01.2013
 *      Author: myaut
 */

#include <defs.h>
#include <mempool.h>

#include <stdlib.h>
#include <string.h>

/**
 * Use standard library allocator for mempool
 * It is faster than mempool, but not autonomous
 * */

#ifdef MEMPOOL_USE_LIBC_HEAP

void mp_cache_init_impl(mp_cache_t* cache, const char* name, size_t item_size) {
	cache->c_item_size = item_size;

	strncpy(cache->c_name, name, MPCACHENAMELEN);
}

void mp_cache_destroy(mp_cache_t* cache) {
	/* Nothing */
}

void* mp_cache_alloc_array(mp_cache_t* cache, unsigned num) {
	return malloc(num * cache->c_item_size);
}

void mp_cache_free_array(mp_cache_t* cache, void* array, unsigned num) {
	free(array);
}

void* mp_cache_alloc(mp_cache_t* cache) {
	return mp_cache_alloc_array(cache, 1);
}

void mp_cache_free(mp_cache_t* cache, void* ptr) {
	return mp_cache_free_array(cache, ptr, 1);
}

void* mp_malloc(size_t sz) {
	return malloc(sz);
}

void* mp_realloc(void* oldptr, size_t sz) {
	return realloc(oldptr, sz);
}

void mp_free(void* ptr) {
	free(ptr);
}

int mempool_init(void) {
	return 0;
}

void mempool_fini(void) {

}

#endif
