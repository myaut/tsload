
/*
    This file is part of TSLoad.
    Copyright 2012-2013, Sergey Klyaus, ITMO University

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



#ifndef MEMPOOL_H_
#define MEMPOOL_H_

#include <tsload/defs.h>

#include <tsload/list.h>
#include <tsload/time.h>
#include <tsload/threads.h>

#include <stdlib.h>
#include <stddef.h>

#define MPCACHENAMELEN		32

struct mp_cache_page;

typedef struct mp_cache {
	thread_mutex_t	c_page_lock;
	thread_rwlock_t	c_list_lock;

	list_head_t	c_page_list;
	struct mp_cache_page* c_last_page;

	size_t		c_item_size;
	unsigned 	c_items_per_page;

	ptrdiff_t	c_first_item_off;

	char		c_name[MPCACHENAMELEN];
} mp_cache_t;

LIBEXPORT void mp_cache_init_impl(mp_cache_t* cache, const char* name, size_t item_size);
LIBEXPORT void mp_cache_destroy(mp_cache_t* cache);

LIBEXPORT void* mp_cache_alloc(mp_cache_t* cache);
LIBEXPORT void mp_cache_free(mp_cache_t* cache, void* ptr);

LIBEXPORT void* mp_cache_alloc_array(mp_cache_t* cache, unsigned num);
LIBEXPORT void mp_cache_free_array(mp_cache_t* cache, void* array, unsigned num);

#define mp_cache_init(cache, type)		mp_cache_init_impl(cache, #type, sizeof(type))

LIBEXPORT void* mp_malloc(size_t sz);
LIBEXPORT void* mp_realloc(void* old, size_t sz);
LIBEXPORT void mp_free(void* ptr);

LIBEXPORT int mempool_init(void);
LIBEXPORT void mempool_fini(void);

PLATAPI void* plat_mp_seg_alloc(size_t seg_size);
PLATAPI int plat_mp_seg_free(void* seg, size_t seg_size);

#endif /* MEMPOOL_H_ */

