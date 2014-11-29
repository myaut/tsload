
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, Tune-IT

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

#ifndef MEMPOOL_IMPL_H_
#define MEMPOOL_IMPL_H_

#include <tsload/defs.h>

#include <tsload/list.h>
#include <tsload/atomic.h>

#define MPSEGMENTSIZE		(2 * SZ_MB)
#define MPPAGESIZE			(8 * SZ_KB)
#define MPPAGELOG			13			/*log2(MPPAGESIZE)*/
#define MPWAITTIME			(1 * T_US)
#define MPMAXRETRIES		3

#define MPREGMASK(num, bidx) 	(((1 << num) - 1) << bidx)

#define MP_BITMAP_BITS		32
#define MP_BITMAP_BUSY		0xFFFFFFFFl

#define MPFRAGSIZE			(MPPAGESIZE / MP_BITMAP_BITS)
#define MPFRAGMASK			(MPFRAGSIZE - 1)

#define FRAG_UNALLOCATED -2
#define FRAG_REFERENCE	 -1

#define FRAG_COMMON		0
#define FRAG_SLAB		1
#define FRAG_HEAP		2

typedef struct {
	signed short fd_type;

	union {
		unsigned short fd_size;
		unsigned short fd_offset;	/* for FRAG_REFERENCE */
	};
} mp_frag_descr_t;

/* Heap allocator */

/* All heap allocations aligned to 8 bytes and maximum allocation is 256 bytes*/
#define MPHEAPUNIT				4
#define MPHEAPMINUNITS			4
#define MPHEAPMAXUNITS			1024

#define MPHEAPMAXALLOC			(800 * MPHEAPUNIT)

/* Number of free fragments to start defragmentation */
#define MPHHDEFRAGFREE			16
/* Period between defragmentations */
#define MPHHDEFRAGPERIOD		8

/* GCC correctly packs mp_heap_header, so size of it would be 32 bits (one unit)
 * on other platforms (such as MSVC), it may create 48-bits structure, so reserve
 * two units for it  */
#ifdef __GNUC__
#define MPHEAPHHSIZE			MPHEAPUNIT
#else
#define MPHEAPHHSIZE			(2 * MPHEAPUNIT)
#endif

#define MPHEAPPAGESIZE			MPHEAPUNIT * MPHEAPMAXUNITS

struct mp_heap_header {
	uint16_t	hh_size	 : 10;

	int16_t		hh_left  : 11;
	int16_t		hh_right : 11;
} PACKED_STRUCT;

typedef struct mp_heap_header mp_heap_header_t;

#define MP_HEAP_HEADER_INIT(hh, free)  	\
		{								\
			hh->hh_size = free ;		\
			hh->hh_left = 0;			\
			hh->hh_right = 0;			\
		}

#define MP_HH_CHILD(hh, direction) ((mp_heap_header_t*) (((char*) (hh)) + ((hh)->hh_##direction) * MPHEAPUNIT))
#define MP_HH_SET_CHILD(hh, child, direction) (hh)->hh_##direction = ((((char*) (child)) - ((char*) (hh))) / MPHEAPUNIT)
#define MP_HH_COPY_CHILD(from, to, direction) MP_HH_SET_CHILD(to, (((from)->hh_##direction == 0) ? to : MP_HH_CHILD(from, direction)), direction)

#define MP_HH_LEFT(hh) 		MP_HH_CHILD(hh, left)
#define MP_HH_RIGHT(hh) 	MP_HH_CHILD(hh, right)
#define MP_HH_NEXT(hh)		MP_HH_CHILD(hh, size)

#define MP_HH_SET_LEFT(hh, child) 		MP_HH_SET_CHILD(hh, child, left)
#define MP_HH_SET_RIGHT(hh, child) 		MP_HH_SET_CHILD(hh, child, right)

#define MP_HH_COPY_LEFT(from, to) 		MP_HH_COPY_CHILD(from, to, left)
#define MP_HH_COPY_RIGHT(from, to) 		MP_HH_COPY_CHILD(from, to, right)

#define MP_HH_TO_FRAGMENT(hh)		(((char*) (hh)) + MPHEAPHHSIZE)
#define MP_HH_FROM_FRAGMENT(ptr)	(mp_heap_header_t*) (((char*) (ptr)) - MPHEAPHHSIZE)

#define MP_HH_IN_PAGE(hh, page)		(((char*) (hh)) > ((char*) (page))) && (((char*) (hh)) < (((char*) (page)) + MPHEAPPAGESIZE))

#define MP_HH_MARK_ALLOCATED(hh)		hh->hh_right = hh->hh_left = (MPHEAPMAXUNITS - 1)
#define MP_HH_IS_ALLOCATED(hh)		(hh->hh_right == (MPHEAPMAXUNITS - 1)) && (hh->hh_left == (MPHEAPMAXUNITS - 1))

#define MP_HH_N		-1
#define MP_HH_L		0
#define MP_HH_R		1

#define MP_HEAP_PAGE_LIFETIME		T_SEC

typedef struct {
	thread_mutex_t		hp_mutex;

	size_t				hp_free;

	list_node_t			hp_node;			/**< List node (for linking with another pages)*/

	ts_time_t			hp_birth;			/**< Time of page allocation */

	int					hp_defrag_period;
	unsigned			hp_allocated_count;
	unsigned			hp_free_count;		/**< Number of free fragments*/
	mp_heap_header_t*	hp_root_free;		/**< Root of free BST */

	char				hp_data[0];
} mp_heap_page_t;

/* SLAB allocator */

#define MPCACHEPAGESIZE		8192

#define MP_CACHE_ITEM(cache, page, index)	 (void*) (((char*) page->cp_first_item) + index * cache->c_item_size)
#define MP_CACHE_ITEM_IN_PAGE(page, ptr)	 (((char*) (ptr)) > ((char*) (page))) && (((char*) (ptr)) < (((char*) (page)) + MPCACHEPAGESIZE))
#define MP_CACHE_ITEM_INDEX(page, ptr)		 ( ( ((char*) ptr) - ((char*) page->cp_first_item) ) / page->cp_cache->c_item_size)

struct mp_cache;

typedef struct {
	struct mp_cache*	cp_cache;
	list_node_t	cp_node;

	atomic_t	cp_free_items;
	void* 		cp_first_item;

	atomic_t	cp_bitmap[0];
} mp_cache_page_t;

#endif /* MEMPOOL_IMPL_H_ */
