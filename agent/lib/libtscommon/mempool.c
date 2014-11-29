
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

#include <tsload/defs.h>

#define LOG_SOURCE		"mempool"
#include <tsload/log.h>

#include <tsload/mempool.h>
#include <tsload/atomic.h>
#include <tsload/time.h>
#include <tsload/list.h>
#include <tsload/threads.h>

#include <mempool.h>

#include <stdlib.h>
#include <assert.h>


#ifndef MEMPOOL_USE_LIBC_HEAP

void* 	mp_segment = NULL;
LIBEXPORT size_t	mp_segment_size = MPSEGMENTSIZE;

atomic_t*				mp_frag_bitmap;
mp_frag_descr_t*		mp_frags;

thread_mutex_t	heap_page_mutex;
thread_rwlock_t	heap_list_lock;
list_head_t heap_page_list;
mp_heap_page_t* last_heap_page;

#ifdef MEMPOOL_TRACE
boolean_t		mp_trace_allocator = B_FALSE;
boolean_t		mp_trace_bitmaps = B_FALSE;
boolean_t		mp_trace_heap = B_FALSE;
boolean_t		mp_trace_heap_btree = B_FALSE;
boolean_t		mp_trace_slab = B_FALSE;
#endif

/* Valgrind integration */
#if defined(HAVE_VALGRIND_VALGRIND_H) && defined(MEMPOOL_VALGRIND)
#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>

#define MEMPOOL_REDZONE_SIZE	0

#else
#define VALGRIND_MEMPOOL_ALLOC(pool, addr, size)
#define VALGRIND_MEMPOOL_FREE(pool, addr)

#define VALGRIND_MAKE_MEM_DEFINED(addr, size)
#define VALGRIND_MAKE_MEM_UNDEFINED(addr, size)

#define VALGRIND_CREATE_MEMPOOL(pool, rzB, is_zeroed)
#define VALGRIND_DESTROY_MEMPOOL(pool)

#define MEMPOOL_REDZONE_SIZE	0
#endif

/**
 * Memory pool
 *
 * When load is concerning on using large bunch of memory, loader itself
 * can starve of memory and so it will fail. To address this issue, we allocate
 * huge amount of memory called "segment", and using our own allocators.
 *
 * There are three allocators:
 * - Page-frag allocator that allocates memory in 256-bytes fragments.
 * - SLAB-like allocator
 * - Traditional heap for small allocations (4 bytes - 256 bytes)
 *
 * SLAB-allocator and Page-frag allocator are using bitmaps to track allocated areas
 */

/**
 * Bitmaps handling.
 * ----------------
 *
 * They are used to track allocated regions of memory and based on atomics.
 *
 * Each bitmap is an array of 32-bit atomic values called cells, and each
 * bit in this atomic is called value and mapped to one small memory region
 * of constant size. For page-frag allocator it's MPFRAGSIZE (1 fragment),
 * while entire cell represents MPPAGESIZE (1 page)
 *
 * When we try to allocate region, we mark entire region with MP_BITMAP_BUSY
 * atomically, so other allocators will think that there is no regions
 * available here and will try next atomic-based bitmap. */

static int mp_bitmap_bitcount(unsigned long u)
{
		/* From http://tekpool.wordpress.com/category/bit-count/ */
        unsigned long count;

        count = u
                 - ((u >> 1) & 033333333333)
                 - ((u >> 2) & 011111111111);
        return
          ((count + (count >> 3))
           & 030707070707) % 63;
}

static int mp_bitmap_find_one(long u) {
	/* From http://skalkoto.blogspot.ru/2008/01/bit-operations-find-first-zero-bit.html */
	u = ~u;
	return mp_bitmap_bitcount((u & (-u)) - 1);
}

/**
 * Find sequence of num zeroes in u
 *
 * @return -1 if failed to do this or index of region */
static int mp_bitmap_find_region(long u, int num) {
	int bidx = 0;
	int i = 0;

	while(i < (MP_BITMAP_BITS - num)) {
		/* Find next zero */
		bidx = mp_bitmap_find_one(u);

		if((u & MPREGMASK(num, bidx)) == 0)
			return bidx;

		i += bidx + 1;
	}

	return -1;
}

/**
 * Allocate num items from cell
 *
 * This function is MT-safe, but if somebody already using cell
 * it will fail and return -1
 *
 * @param cell Cell where allocation proceeds
 * @param num Number of items to allocate
 * @param max_items Total number of items in cell
 *
 * @return index if item inside cell or -1 if allocation was unsuccessfull
 *  */
int mp_bitmap_cell_alloc(atomic_t* cell, int num, int max_items) {
	long bval;
	long mask;
	int bidx = -1;

	bval = atomic_exchange(cell, MP_BITMAP_BUSY);

	if(bval != MP_BITMAP_BUSY) {
		/* There are free slots in this cell*/
		if(num == 1) {
			bidx = mp_bitmap_find_one(bval);
			mask = (1 << bidx);
		}
		else if(num == MP_BITMAP_BITS) {
			/* Need to allocate everything from cell, so
			 * simply check if all bval bits are zeroes */
			bidx = (bval == 0) ? 0 : -1;
			mask = MP_BITMAP_BUSY;
		}
		else {
			bidx = mp_bitmap_find_region(bval, num);
			mask = MPREGMASK(num, bidx);
		}

		if((bidx + num) > max_items) {
			/* bidx too large */
			bidx = -1;
		}

		if(bidx != -1) {
#			ifdef MEMPOOL_TRACE
			if(mp_trace_bitmaps) {
				logmsg(LOG_TRACE, "ALLOC BITMAP @%p [%d:%d] %08lx->%08lx",
						cell, bidx, bidx + num, bval, bval | mask);
			}
#			endif

			/* If allocation was successful, set new bval
			 * Otherwise we simply restore old value */
			bval |= mask;
		}

		/* If somebody freed/alloced memory than bitmap cell may contain zeroes.
		 * So we shouldn't write bval directly, but do and operation*/
		atomic_and(cell, bval);
	}

	/* If bval was MP_BITMAP_BUSY, no need to recover value*/

	return bidx;
}

/**
 * Allocate items from bitmap
 *
 * If allocation would be unsuccessful, it will retry up to MPMAXRETRIES times
 * Also it sleeps between retries for MPWAITTIME
 *
 * @param bitmap Array of bitmap cells
 * @param num_items 	Total number of items in bitmap
 * @param alloc_items	Number of items to allocate
 *
 * @return Index of allocated item or -1 if all tries was failed */
int mp_bitmap_alloc(atomic_t* bitmap, int num_items, int alloc_items) {
	int i = 0;
	int retries = 0;
	int bidx = 0;

	/* For SLAB-allocator last cell may contain less than MP_BITMAP_BITS items
	 * So, calculate last_items (number of items in last cell). */
	int num_cells = num_items / MP_BITMAP_BITS;
	int last_items = num_items % MP_BITMAP_BITS;
	int num_items_in_cell;

	if(last_items != 0) {
		++num_cells;
	}
	else {
		last_items = MP_BITMAP_BITS;
	}

	/* Allocate item*/
	while(retries < MPMAXRETRIES) {
		for(i = 0; i < num_cells; ++i) {
			num_items_in_cell = (i == (num_cells - 1)) ? last_items : MP_BITMAP_BITS;
			bidx = mp_bitmap_cell_alloc(bitmap + i, alloc_items, num_items_in_cell);

			if(bidx != -1) {
				return i * MP_BITMAP_BITS + bidx;
			}
		}

		tm_sleep_nano(MPWAITTIME);

		++retries;
	}

	return -1;
}

/**
 * Allocate multiple cells from bitmap
 *
 * @param bitmap Array of bitmap cells
 * @param num_items 	Total number of items in bitmap
 * @param alloc_items	Number of items to allocate
 *
 * @return Index of allocated item or -1 if all tries was failed */
int mp_bitmap_alloc_cells(atomic_t* bitmap, int num_items, int alloc_cells) {
	int i = 0, j = 0;
	int retries = 0;
	int bidx = 0;

	int num_cells = num_items / MP_BITMAP_BITS;

	int first_cell = -1;
	int allocated_cells = 0;

	while(retries < MPMAXRETRIES) {
		for(i = 0; i < num_cells; ++i) {
			/* Try to allocate entire cell */
			bidx = mp_bitmap_cell_alloc(bitmap + i, MP_BITMAP_BITS, MP_BITMAP_BITS);

			if(bidx != -1) {
				/* Allocation successfull */
				if(first_cell == -1) {
					first_cell = i;
				}
				else {
					++allocated_cells;

					if(allocated_cells == alloc_cells)
						return first_cell * MP_BITMAP_BITS;
				}
			}
			else {
				/* Allocation was unsuccessfull: free allocated cells
				 * and try again */
				if(first_cell != -1) {
					for(j = first_cell; j <= i ; ++j) {
						atomic_set(bitmap + j, 0l);
					}

					allocated_cells = 0;
					first_cell = -1;
				}
			}
		}

		tm_sleep_nano(MPWAITTIME);

		++retries;
	}

	return -1;
}

void mp_bitmap_free(atomic_t* bitmap, int num, int idx) {
	int bidx = idx % MP_BITMAP_BITS;
	int i = idx / MP_BITMAP_BITS;

	long mask = MPREGMASK(num, bidx);
	long bval;

	bitmap += i;

	/* Clear bits */
	bval = atomic_and(bitmap, ~mask);

#	ifdef MEMPOOL_TRACE
	if(mp_trace_bitmaps) {
		logmsg(LOG_TRACE, "FREE BITMAP @%p [%d:%d] %08lx->%08lx",
				bitmap, bidx, bidx + num, bval, bval & ~mask);
	}
#	endif
}

void mp_bitmap_free_cells(atomic_t* bitmap, int num, int idx) {
	int i = 0;
	long bval;

	idx /= MP_BITMAP_BITS;
	num /= MP_BITMAP_BITS;

	for(i = 0; i < num ; ++i) {
		bval = atomic_exchange(bitmap + i, 0l);

#		ifdef MEMPOOL_TRACE
		if(mp_trace_bitmaps) {
			logmsg(LOG_TRACE, "FREE BITMAP @%p [0:%d] %08lx->%08lx",
					bitmap + i, MP_BITMAP_BITS, bval, 0l);
		}
#		endif
	}
}

/*
 * Page-frag allocator
 * -------------------*/

void mp_frag_allocator_init() {
	int num_pages = mp_segment_size / MPPAGESIZE;
	int num_frags = num_pages * MP_BITMAP_BITS;
	int i = 0, j = 0;

	/* Use libc's heap. We need only 32 byte for this (for segsize = 2M and pagesize = 8K),
	 * so it will be wasteful to use entire page for this internal data structure*/
	mp_frag_bitmap = (atomic_t*) malloc(num_pages * sizeof(atomic_t));

	/* mp_frag_size contains size of fragment */
	mp_frags = (mp_frag_descr_t*) malloc(num_frags * sizeof(mp_frag_descr_t));

	/* No need to use atomic operations here */
	for(i = 0; i < num_pages; ++i) {
		mp_frag_bitmap[i] = (atomic_t) 0l;

		for(j = 0; j < MP_BITMAP_BITS; ++j) {
			mp_frags[i * MP_BITMAP_BITS + j].fd_type = FRAG_UNALLOCATED;
		}
	}
}

void mp_frag_allocator_destroy() {
	free(mp_frag_bitmap);

	free(mp_frags);
}

/**
 * Allocate page fragment, entire page or multiple pages
 *
 * If appropriate fragment couldn't be found, aborts
 *
 * @param size number of bytes to allocate
 * @param flags fragment flags FRAG_COMMON, FRAG_SLAB or FRAG_HEAP
 *
 * @return pointer to fragment*/
void* mp_frag_alloc(size_t size, short flags) {
	int alloc_frags = (size / MPFRAGSIZE) + ((size % MPFRAGSIZE) != 0);
	int num_frags = (mp_segment_size / MPPAGESIZE) * MP_BITMAP_BITS;

	int idx;

	int i = 0;
	mp_frag_descr_t* descr;

	void* ptr;

	assert(flags >= 0);

	if(alloc_frags <= MP_BITMAP_BITS) {
		/* Single page or fragment*/
		idx = mp_bitmap_alloc(mp_frag_bitmap, num_frags, alloc_frags);
	}
	else {
		/* Multiple pages */
		int alloc_pages = (alloc_frags / MP_BITMAP_BITS) + ((alloc_frags % MP_BITMAP_BITS) != 0);

		idx = mp_bitmap_alloc_cells(mp_frag_bitmap, num_frags, alloc_pages);
	}

	if(idx == -1) {
		/*TODO: should implement redzone for critical allocs */
		logmsg(LOG_CRIT, "Page allocator failed allocate a page! Abort.");
		abort();
	}

	/* Set up fragment descriptors */

	descr = mp_frags + idx;

	assert(descr->fd_type == FRAG_UNALLOCATED);

	descr->fd_type = flags;
	descr->fd_size = alloc_frags;

	if(alloc_frags > 1) {
		++descr;

		for(i = 1; i < alloc_frags; ++i, ++descr) {
			assert(descr->fd_type == FRAG_UNALLOCATED);

			descr->fd_type = FRAG_REFERENCE;
			descr->fd_offset = i;
		}
	}

	/* Compute pointer and return it */
	ptr = ((char*) mp_segment) + (idx * MPFRAGSIZE);

	if(flags == FRAG_COMMON)
		VALGRIND_MEMPOOL_ALLOC(mp_segment, ptr, size);


#	ifdef MEMPOOL_TRACE
	if(mp_trace_allocator) {
		logmsg(LOG_TRACE, "ALLOC FRAG %d frags (size: %zd) from %d -> %p", alloc_frags, size, idx, ptr);
	}
#	endif

	return ptr;
}

void mp_frag_free(void* frag) {
	int idx = (((char*) frag) - ((char*) mp_segment)) / MPFRAGSIZE;
	int free_frags;

	int i = 0;
	mp_frag_descr_t* descr;

	/* Freeing wrong fragment */
	assert((idx >= 0) && (idx < ((mp_segment_size / MPPAGESIZE) * MP_BITMAP_BITS)));

	descr = mp_frags + idx;

	if(descr->fd_type < 0) {
		if(descr->fd_type == FRAG_UNALLOCATED) {
			logmsg(LOG_CRIT, "Trying to free memory @%p which is unallocated", frag);
		}
		else if(descr->fd_type == FRAG_REFERENCE) {
			logmsg(LOG_CRIT, "Trying to free memory @%p which is part of fragment [%p",
					frag, ((char*) frag) - (descr->fd_size * MPFRAGSIZE));
		}

		return;
	}
	free_frags = descr->fd_size;

	if(descr->fd_type == FRAG_COMMON)
		VALGRIND_MEMPOOL_FREE(mp_segment, frag);

	/* Clear fragment descriptors */
	for(i = 0; i < free_frags; ++i, ++descr) {
		descr->fd_type = FRAG_UNALLOCATED;
	}

	if(free_frags <= MP_BITMAP_BITS) {
		mp_bitmap_free(mp_frag_bitmap, free_frags, idx);
	}
	else {
		mp_bitmap_free_cells(mp_frag_bitmap, free_frags, idx);
	}

#	ifdef MEMPOOL_TRACE
	if(mp_trace_allocator) {
		logmsg(LOG_TRACE, "FREE FRAG %d frags from %d <- %p", free_frags, idx, frag);
	}
#	endif
}

mp_frag_descr_t* mp_frag_get_descr(void* frag) {
	int idx = (((char*) frag) - ((char*) mp_segment)) / MPFRAGSIZE;
	mp_frag_descr_t* descr = mp_frags + idx;

	if(descr->fd_type == FRAG_UNALLOCATED) {
		return NULL;
	}

	if(descr->fd_type == FRAG_REFERENCE) {
		return descr - descr->fd_offset;
	}

	return descr;
}

size_t mp_frag_get_size(mp_frag_descr_t* descr) {
	return descr->fd_size * MPFRAGSIZE;
}

/* Traditional heap
 * ----------------
 *
 * In traditional heap all objects allocated in fragments of varying size. However it has
 * several predefined limitations:
 * 	- All allocations are aligned to unit size MPHEAPUNIT
 * 	- At least MPHEAPMINUNITS should be allocated, and maximum is MPHEAPMAXUNITS
 *
 * 	Heap consists of list of heap pages heap_page_list that is protected by rwlock heap_list_lock
 * 	Each page has page header, and fragment headers. Fragment headers for free areas form
 * 	binary search tree (free BST), so mempool heap uses best fit policy for allocating chunks */

void mp_heap_allocator_init() {
	list_head_init(&heap_page_list, "heap_page_list");

	mutex_init(&heap_page_mutex, "heap_page");
	rwlock_init(&heap_list_lock, "heap_list");
}

void mp_heap_allocator_destroy() {
	rwlock_destroy(&heap_list_lock);
	mutex_destroy(&heap_page_mutex);
}

/**
 * Allocate new heap page, initialize it. If somebody already allocating page,
 * it will wait until it's done than return  last_heap_page
 *
 * @return allocated page */
mp_heap_page_t* mp_heap_page_alloc() {
	void* raw_page = NULL;
	mp_heap_page_t* page = NULL;
	mp_heap_header_t* header;

	size_t free;

	while(page == NULL) {
		if(mutex_try_lock(&heap_page_mutex)) {
			raw_page = mp_frag_alloc(MPHEAPPAGESIZE, FRAG_HEAP);

			/* Initalize heap's page */
			page = (mp_heap_page_t*) raw_page;
			header = (mp_heap_header_t*) &(page->hp_data);

			VALGRIND_MAKE_MEM_DEFINED(page, sizeof(mp_heap_page_t));

			free = (MPHEAPPAGESIZE - sizeof(mp_heap_page_t) - MPHEAPHHSIZE) / MPHEAPUNIT;

			last_heap_page = page;

			mutex_init(&page->hp_mutex, "hp-mutex-%p", (void*) raw_page);

			page->hp_free = free;
			page->hp_root_free = header;
			page->hp_allocated_count = 0;
			page->hp_free_count = 1;
			page->hp_defrag_period = 1;

			page->hp_birth = tm_get_time();

			/* Initialize header */
			VALGRIND_MAKE_MEM_DEFINED(header, MPHEAPHHSIZE);
			MP_HEAP_HEADER_INIT(header, free);

			rwlock_lock_write(&heap_list_lock);
			list_add(&page->hp_node, &heap_page_list);
			rwlock_unlock(&heap_list_lock);

			mutex_unlock(&heap_page_mutex);
		}
		else {
			/* Somebody allocating or freeing page now, wait on mutex than return
			 * last allocated heap page. If last_heap_page will be freed, page
			 * would be set to NULL, and we loop again */
			mutex_lock(&heap_page_mutex);
			page = last_heap_page;
			mutex_unlock(&heap_page_mutex);
		}
	}

#	ifdef MEMPOOL_TRACE
	if(mp_trace_heap) {
		logmsg(LOG_TRACE, "ALLOC HEAP PAGE %p", page);
	}
#	endif

	return page;
}

/**
 * Free heap page
 */
void mp_heap_page_free(mp_heap_page_t* page) {
	/* Do not free page if it was nearly allocated */
	if((tm_get_time() - page->hp_birth) < MP_HEAP_PAGE_LIFETIME)
		return;

	mutex_lock(&heap_page_mutex);
	rwlock_lock_write(&heap_list_lock);

	if(last_heap_page == page)
		last_heap_page = NULL;

	list_del(&page->hp_node);

	rwlock_unlock(&heap_list_lock);
	mutex_unlock(&heap_page_mutex);

#	ifdef MEMPOOL_TRACE
	if(mp_trace_heap) {
		logmsg(LOG_TRACE, "FREE HEAP PAGE %p", page);
	}
#	endif

	mp_frag_free(page);

	VALGRIND_MAKE_MEM_UNDEFINED(page, sizeof(mp_heap_page_t));
}

/**
 * Linearize free BST from root into free starting from index recursively
 *
 * @param free Free list
 * @param root Root entry of free BST (may be subtree root for recursive calls)
 * @param index Index from which linearization should come (for recursive calls)*/
static int mp_heap_btree_linearize(mp_heap_header_t** free, mp_heap_header_t* root, int index) {
	mp_heap_header_t* left = MP_HH_LEFT(root);
	mp_heap_header_t* right = MP_HH_RIGHT(root);

	free[index++] = root;

	if(left != root)
		index = mp_heap_btree_linearize(free, left, index);

	if(right != root)
		index = mp_heap_btree_linearize(free, right, index);

	return index;
}

static int mp_heap_header_lcmp(const void* hh1, const void* hh2) {
	/* hh is mp_heap_header_t**, but we cast pointer to unsigned long*/

	unsigned long* ptr1 = (unsigned long*) hh1;
	unsigned long* ptr2 = (unsigned long*) hh2;

	return ((int) (*ptr1 & 0xFFFF)) - ((int) (*ptr2 & 0xFFFF));
}

void mp_heap_btree_insert(mp_heap_header_t** root, mp_heap_header_t* hh);

/**
 * Defragment free BST.
 *
 * Linearizes free BST, finds sibling free fragments and enlarges them.
 * Uses mp_heap_btree_insert to form new free BST */
void mp_heap_page_defrag(mp_heap_page_t* page) {
	mp_heap_header_t** free = NULL;
	unsigned free_count = page->hp_free_count;
	unsigned lin_count = 0;
	int index = 0;
	mp_heap_header_t *hh, *hh_next;

	/* No need to defragment now */
	if(--page->hp_defrag_period > 0)
		return;

#		ifdef MEMPOOL_TRACE
		if(mp_trace_heap_btree) {
			logmsg(LOG_TRACE, "HEAP BST DEFRAG page: %p free: %d", page, page->hp_free_count);
		}
#		endif

	free = (mp_heap_header_t**) mp_frag_alloc(page->hp_free_count * sizeof(void*), FRAG_COMMON);

	/* Linearize free BST to free */
	lin_count = mp_heap_btree_linearize(free, page->hp_root_free, 0);
	assert(lin_count == free_count);

	/* Sort free list */
	qsort(free, free_count, sizeof(void*), mp_heap_header_lcmp);

	/* Remove BST and create new one */
	page->hp_root_free = NULL;
	page->hp_free_count = 0;

	while(index < free_count) {
		hh = free[index];
		hh_next = MP_HH_NEXT(hh);

		++index;

		/* Merge sibling free entries */
		while(index < free_count &&
			  hh_next == free[index]) {
			hh->hh_size += hh_next->hh_size;

			hh_next = MP_HH_NEXT(hh_next);
			++index;

			VALGRIND_MAKE_MEM_UNDEFINED(hh_next, MPHEAPHHSIZE);
		}

		/* Insert entry back to free BST */
		mp_heap_btree_insert(&page->hp_root_free, hh);
		++page->hp_free_count;
	}

#		ifdef MEMPOOL_TRACE
		if(mp_trace_heap_btree) {
			logmsg(LOG_TRACE, "HEAP BST DEFRAG END page: %p freed %d frags",
					page, (page->hp_free_count - free_count));
		}
#		endif

	mp_frag_free(free);

	page->hp_defrag_period = MPHHDEFRAGPERIOD;
}

#ifdef MEMPOOL_TRACE
#define DUMP_TRACE_LEN	4096

void mp_heap_btree_dump(char* str, int* len, mp_heap_header_t* hh) {
	char preamble[15];

	mp_heap_header_t *left = MP_HH_LEFT(hh);
	mp_heap_header_t *right = MP_HH_RIGHT(hh);

	sprintf(preamble, "%06x,%d:(", ((unsigned) hh) & 0xFFFFFF, hh->hh_size);

	*len += 15;
	if(*len > DUMP_TRACE_LEN) {
		strcat(str, "...");
		return;
	}

	strcat(str, preamble);
	if(left != hh)
		mp_heap_btree_dump(str, len, left);
	strcat(str, ";");
	if(right != hh)
		mp_heap_btree_dump(str, len, right);
	strcat(str, ")");
}

void mp_heap_btree_print(mp_heap_header_t* root) {
	char mp_heap_dump[4096];
	int len = 0;

	mp_heap_dump[0] = '\0';

	if(root !=  NULL) {
		mp_heap_btree_dump(mp_heap_dump, &len, root);
		logmsg(LOG_TRACE, "HEAP BST TREE %p %s", root, mp_heap_dump);
	}
	else {
		logmsg(LOG_TRACE, "HEAP BST TREE EMPTY");
	}
}
#endif

/**
 * Insert entry hh to free BST
 *
 * TODO: According to mpbench measurments it's hotspot, should be optimized
 *
 * @param root Pointer to root of tree (May be altered only if NULL)
 * @param hh Entry to be inserted
 * */
void mp_heap_btree_insert(mp_heap_header_t** root, mp_heap_header_t* hh) {
	mp_heap_header_t *parent = *root;
	mp_heap_header_t *left = NULL;
	mp_heap_header_t *right = NULL;

	boolean_t inserted = B_FALSE;

	hh->hh_left = 0;
	hh->hh_right = 0;

#	ifdef MEMPOOL_TRACE
	if(mp_trace_heap_btree) {
		logmsg(LOG_TRACE, "HEAP BST INSERT %p", hh);
		mp_heap_btree_print(*root);
	}
#	endif

	if(*root == NULL) {
		*root = hh;

#		ifdef MEMPOOL_TRACE
		if(mp_trace_heap_btree) {
			logmsg(LOG_TRACE, "HEAP BST ROOT %p (sz: %d)", hh, hh->hh_size);
		}
#		endif

		return;
	}

	while(!inserted) {
		if(hh->hh_size > parent->hh_size) {
			right = MP_HH_RIGHT(parent);

			if(right == parent) {
				MP_HH_SET_RIGHT(parent, hh);
				inserted = B_TRUE;
			}
			else {
				parent = right;
			}
		}
		else if(hh->hh_size < parent->hh_size) {
			left = MP_HH_LEFT(parent);

			if(left == parent) {
				MP_HH_SET_LEFT(parent, hh);
				inserted = B_TRUE;
			}
			else {
				parent = left;
			}
		}
		else {
			/* hh->hh_size == parent->hh_size */
			MP_HH_COPY_LEFT(parent, hh);
			MP_HH_SET_LEFT(parent, hh);
			inserted = B_TRUE;
		}
	}

#	ifdef MEMPOOL_TRACE
	if(mp_trace_heap_btree) {
		logmsg(LOG_TRACE, "HEAP BST INSERT %p (p: %p pl: %p pr: %p sz: %d)",
				hh, parent, MP_HH_LEFT(parent), MP_HH_RIGHT(parent), hh->hh_size);
		mp_heap_btree_print(*root);
	}
#	endif
}

/**
 * Removes element from free BST
 *
 * @param root Pointer to root of tree (may be altered)
 * @param parent Parent of hh
 * @param hh Header to remove
 * @param direction Direction from parent to hh:
 * 		MP_HH_N - No direction (hh is root and pparent == &root)
 * 		MP_HH_L	- parent --(left)---> hh
 * 		MP_HH_R - parent --(right)--> hh
 * */
void mp_heap_btree_delete(mp_heap_header_t** root, mp_heap_header_t* parent, mp_heap_header_t* hh, int direction) {
	mp_heap_header_t *left = MP_HH_LEFT(hh);
	mp_heap_header_t *right = MP_HH_RIGHT(hh);

	mp_heap_header_t *tail = NULL;
	mp_heap_header_t *tparent = NULL;

	/* Direction from tparent to tail */
	int tdir = MP_HH_R;

	if(left != hh && right != hh) {
		/* hh has two children */
		tparent = hh;
		tail = right;

		/* Find smallest successor with no children (tail)
		 * If there is no right subtree, right == hh, so we will search in left subtree*/
		while(tail->hh_left != 0) {
			tparent = tail;
			tail = MP_HH_LEFT(tail);

			tdir = MP_HH_L;
		}

		/* Remove tail from tree */
		mp_heap_btree_delete(root, tparent, tail, tdir);

		MP_HH_COPY_LEFT(hh, tail);
		MP_HH_COPY_RIGHT(hh, tail);
	}
	else {
		if(left != hh) tail = left;
		else if(right != hh) tail = right;
		else tail = parent;					/* No children at all, link it to itself */
	}

	switch(direction) {
	case MP_HH_L:
		MP_HH_SET_LEFT(parent, tail);
		break;
	case MP_HH_R:
		MP_HH_SET_RIGHT(parent, tail);
		break;
	case MP_HH_N:
		/* If we are deleting root node, so, parent will be NULL
		 * and *root will be NULL too */
		*root = tail;
		break;
	}

#	ifdef MEMPOOL_TRACE
	if(mp_trace_heap_btree) {
		logmsg(LOG_TRACE, "HEAP BST DELETE %p (p: %p l: %p r: %p sz: %d) dir: %d", hh,
						parent, left, right, hh->hh_size, direction);
		mp_heap_btree_print(*root);
	}
#	endif

	hh->hh_left = 0;
	hh->hh_right = 0;
}

/**
 * Find best match, remove it from free-tree split, and return
 *
 * @param root Pointer to root of btree
 * @param units Minimum size of allocated fragment in units*/
mp_heap_header_t* mp_heap_btree_find_delete(mp_heap_header_t** root, size_t units) {
	mp_heap_header_t *hh = *root, *parent = NULL;
	mp_heap_header_t *left = hh, *right = hh;

	int direction = MP_HH_N;
	int diff = 0;

	/* Best fit variables*/
	mp_heap_header_t *best = *root, *b_parent = NULL;

	int b_diff = (*root)->hh_size - units;
	int b_direction = MP_HH_N;

	while(B_TRUE) {
		left = MP_HH_LEFT(hh);
		right = MP_HH_RIGHT(hh);

		diff = hh->hh_size - units;

		/* If found chunk has enough space (diff >= 0)
		 *  and it is better than previous (diff <= b_diff)
		 *  or previous chunk was not suitable (b_diff < 0) */
		if(diff >= 0 && (diff <= b_diff || b_diff < 0) ) {
			b_parent = parent;
			best = hh;
			b_diff = diff;
			b_direction = direction;

			/* Found best match */
			if(diff == 0)
				break;
		}

		/* Go forward */
		if(left != hh && diff > 0) {
			parent = hh;
			direction = MP_HH_L;
			hh = left;
		}
		else if(right != hh && diff < 0) {
			parent = hh;
			direction = MP_HH_R;
			hh = right;
		}
		else {
			break;
		}
	}

	/* All fragments are too small, return NULL */
	if(b_diff < 0)
		return NULL;

	mp_heap_btree_delete(root, b_parent, best, b_direction);

	return best;
}

/**
 * Allocate area from heap
 *
 * @param size Size of allocation area
 *
 * @return allocated area
 * */
void* mp_heap_alloc(size_t size) {
	/* Align size to heap units */
	size_t units = size / MPHEAPUNIT +
				   ((size % MPHEAPUNIT) != 0) +
				   MPHEAPHHSIZE / MPHEAPUNIT;
	mp_heap_header_t* hh = NULL, *hh_next = NULL;

	mp_heap_page_t* page = NULL;
	boolean_t found_page = B_FALSE;

	size_t diff = 0;

	void* ptr;

	if(unlikely(units < MPHEAPMINUNITS))
		units = MPHEAPMINUNITS;

	/* Try to get page from already allocated heap page list */
	do {
		rwlock_lock_read(&heap_list_lock);

		if(page == NULL)
			page = list_first_entry(mp_heap_page_t, &heap_page_list, hp_node);

		list_for_each_entry_continue(mp_heap_page_t, page, &heap_page_list, hp_node) {
			if(mutex_try_lock(&page->hp_mutex)) {
				if(page->hp_root_free != NULL) {
					found_page = B_TRUE;
					break;
				}

				mutex_unlock(&page->hp_mutex);
			}
		}
		rwlock_unlock(&heap_list_lock);

		if(!found_page) {
			/* Not found page - too much concurrency for heap pages,
			 * or no free space on it - allocate new one */
			page = mp_heap_page_alloc();
			mutex_lock(&page->hp_mutex);

			found_page = B_TRUE;
		}

		hh = mp_heap_btree_find_delete(&page->hp_root_free, units);

		/* Failed to allocate from this page, try new one */
		if(hh == NULL) {
#			ifdef MEMPOOL_TRACE
			if(mp_trace_heap) {
				logmsg(LOG_TRACE, "ALLOC HEAP FAIL @page: %p (%zd free in %d frags)",
						page, page->hp_free, page->hp_free_count);
			}
#			endif

			found_page = B_FALSE;
			mutex_unlock(&page->hp_mutex);
			continue;
		}

		MP_HH_MARK_ALLOCATED(hh);

		diff = hh->hh_size - units;
		++page->hp_allocated_count;
		--page->hp_free_count;

		if(diff > MPHEAPMINUNITS) {
			/* Trim allocated fragment */
			hh->hh_size = units;

			hh_next = MP_HH_NEXT(hh);
			VALGRIND_MAKE_MEM_DEFINED(hh_next, MPHEAPHHSIZE);	/* Say valgrind that hh is defined */
			MP_HEAP_HEADER_INIT(hh_next, diff);

			/* Return rest of fragment to free list */
			++page->hp_free_count;
			mp_heap_btree_insert(&page->hp_root_free, hh_next);
		}

		page->hp_free -= hh->hh_size;

		mutex_unlock(&page->hp_mutex);
	} while(!found_page);

#	ifdef MEMPOOL_TRACE
	if(mp_trace_heap) {
		logmsg(LOG_TRACE, "ALLOC HEAP %zd (%d real) units @page: %p (%zd free in %d frags) <- %p",
				units, hh->hh_size, page, page->hp_free, page->hp_free_count, hh);
	}
#	endif

	ptr = MP_HH_TO_FRAGMENT(hh);

	VALGRIND_MEMPOOL_ALLOC(mp_segment, ptr, size);
	return ptr;
}

/**
 * Free area allocated by heap
 *
 * @param ptr Allocated area
 * */
void mp_heap_free(void* ptr) {
	mp_heap_header_t* hh = MP_HH_FROM_FRAGMENT(ptr);

	mp_heap_page_t* page = NULL, *iter = NULL;

	VALGRIND_MAKE_MEM_DEFINED(hh, MPHEAPHHSIZE);

	assert(hh->hh_size > 0);
	assert(MP_HH_IS_ALLOCATED(hh));

	rwlock_lock_read(&heap_list_lock);
	list_for_each_entry(mp_heap_page_t, iter, &heap_page_list, hp_node) {
		if(MP_HH_IN_PAGE(hh, iter)) {
			page = iter;
			break;
		}
	}
	rwlock_unlock(&heap_list_lock);

	assert(page != NULL);

	/* Return header on free-tree */
	mutex_lock(&page->hp_mutex);

	mp_heap_btree_insert(&page->hp_root_free, hh);

	++page->hp_free_count;
	page->hp_free += hh->hh_size;

	if(--page->hp_allocated_count == 0) {
		mp_heap_page_free(page);
	}
	else {
		if(page->hp_free_count > MPHHDEFRAGFREE)
			mp_heap_page_defrag(page);
	}

	mutex_unlock(&page->hp_mutex);

#	ifdef MEMPOOL_TRACE
	if(mp_trace_heap) {
		logmsg(LOG_TRACE, "FREE HEAP %d units @page: %p (%zd free in %d frags) <- %p",
				hh->hh_size, page, page->hp_free, page->hp_free_count, hh);
	}
#	endif

	VALGRIND_MEMPOOL_FREE(mp_segment, ptr);
}

/**
 * Get size of allocated area from heap
 * */
static size_t mp_heap_get_size(void* ptr) {
	mp_heap_header_t* hh = MP_HH_FROM_FRAGMENT(ptr);

	return hh->hh_size * MPHEAPUNIT;
}

/**
 * SLAB allocator
 * */

mp_cache_page_t* mp_cache_page_alloc(mp_cache_t* cache) {
	mp_cache_page_t* page;
	void* raw_page;

	int i;
	int bitmap_cells;

	/* Same logic as inthread mp_heap_page_alloc */
	if(mutex_try_lock(&cache->c_page_lock)) {
		raw_page = mp_frag_alloc(MPCACHEPAGESIZE, FRAG_SLAB);

		page = (mp_cache_page_t*) raw_page;

		VALGRIND_MAKE_MEM_DEFINED(page, cache->c_first_item_off);

		page->cp_cache = cache;
		atomic_set(&page->cp_free_items, cache->c_items_per_page);

		page->cp_first_item = ((char*) raw_page) + cache->c_first_item_off;

		/* Initialize bitmap */
		bitmap_cells = cache->c_items_per_page / MP_BITMAP_BITS +
					   ((cache->c_items_per_page % MP_BITMAP_BITS) != 0);
		for(i = 0; i < bitmap_cells; ++i)
			page->cp_bitmap[i] = 0l;

		rwlock_lock_write(&cache->c_list_lock);
		list_add(&page->cp_node, &cache->c_page_list);
		cache->c_last_page = page;
		rwlock_unlock(&cache->c_list_lock);

		mutex_unlock(&cache->c_page_lock);
	}
	else {
		mutex_lock(&cache->c_page_lock);
		page = cache->c_last_page;
		mutex_unlock(&cache->c_page_lock);
	}

#	ifdef MEMPOOL_TRACE
	if(mp_trace_slab) {
		logmsg(LOG_TRACE, "ALLOC SLAB PAGE %s %p", cache->c_name, page);
	}
#	endif

	return page;
}

void mp_cache_page_free_nolock(mp_cache_page_t* page) {
	mp_cache_t* cache = page->cp_cache;

	list_del(&page->cp_node);

	if(cache->c_last_page == page) {
		cache->c_last_page = NULL;
	}

	VALGRIND_MAKE_MEM_UNDEFINED(page, cache->c_first_item_off);

#	ifdef MEMPOOL_TRACE
	if(mp_trace_slab) {
		logmsg(LOG_TRACE, "FREE SLAB PAGE %s %p", cache->c_name, page);
	}
#	endif
}

void mp_cache_page_free(mp_cache_page_t* page) {
	rwlock_lock_write(&page->cp_cache->c_list_lock);
	mp_cache_page_free_nolock(page);
	rwlock_unlock(&page->cp_cache->c_list_lock);
}

void mp_cache_init_impl(mp_cache_t* cache, const char* name, size_t item_size) {
	unsigned items_per_page;
	size_t  bitmap_size;
	size_t  svc_size;

	list_head_init(&cache->c_page_list, "cache-%s", name);

	mutex_init(&cache->c_page_lock, "cache-%s", name);
	rwlock_init(&cache->c_list_lock, "cache-%s", name);

	cache->c_item_size = item_size;

	items_per_page = MPCACHEPAGESIZE / item_size;

	bitmap_size = max(sizeof(atomic_t), sizeof(atomic_t) * items_per_page / MP_BITMAP_BITS);
	svc_size = sizeof(mp_cache_page_t) + bitmap_size;

	cache->c_items_per_page = items_per_page;
	cache->c_first_item_off = svc_size;

	while((cache->c_items_per_page * item_size) > (MPCACHEPAGESIZE - svc_size)) {
		--cache->c_items_per_page;
	}

	strncpy(cache->c_name, name, MPCACHENAMELEN);

#	ifdef MEMPOOL_TRACE
	if(mp_trace_slab) {
		logmsg(LOG_TRACE, "NEW SLAB CACHE %s %p ipp: %u",
				cache->c_name, cache, cache->c_items_per_page);
	}
#	endif
}

void mp_cache_destroy(mp_cache_t* cache) {
	mp_cache_page_t *page, *temp;

	rwlock_lock_write(&cache->c_list_lock);
	list_for_each_entry_safe(mp_cache_page_t, page, temp, &cache->c_page_list, cp_node) {
		mp_cache_page_free_nolock(page);
	}
	rwlock_unlock(&cache->c_list_lock);

	mutex_destroy(&cache->c_page_lock);
	rwlock_destroy(&cache->c_list_lock);
}

STATIC_INLINE boolean_t mp_cache_reserve(mp_cache_page_t* page, unsigned num) {
	if(atomic_sub(&page->cp_free_items, num) >= 0) {
		return B_TRUE;
	}

	/* Oops - not enough free items on this page, return it */
	atomic_add(&page->cp_free_items, num);

	return B_FALSE;
}

void* mp_cache_alloc_array(mp_cache_t* cache, unsigned num) {
	boolean_t found_page = B_FALSE;
	mp_cache_page_t *page = NULL;

	void* item = NULL;

	int idx;

	/* Align num */
	if(num > MP_BITMAP_BITS)
		num += MP_BITMAP_BITS - (num % MP_BITMAP_BITS);

	assert(num < cache->c_items_per_page);

	while(!found_page) {
		rwlock_lock_read(&cache->c_list_lock);

		if(page == NULL)
			page = list_first_entry(mp_cache_page_t, &cache->c_page_list, cp_node);

		list_for_each_entry_continue(mp_cache_page_t, page, &cache->c_page_list, cp_node) {
			found_page = mp_cache_reserve(page, num);

			if(found_page)
				break;
		}
		rwlock_unlock(&cache->c_list_lock);

		if(!found_page) {
			page = mp_cache_page_alloc(cache);
			found_page = mp_cache_reserve(page, num);

			/* Coundn't alloc on new page - try again */
			if(!found_page)
				continue;
		}

		if(num <= MP_BITMAP_BITS) {
			idx = mp_bitmap_alloc(page->cp_bitmap, cache->c_items_per_page, num);
		}
		else {
			idx = mp_bitmap_alloc_cells(page->cp_bitmap, cache->c_items_per_page, num / MP_BITMAP_BITS);
		}

		if(idx == -1) {
			/* Failed to alloc array, possibly fragmented page */
			atomic_add(&page->cp_free_items, num);
			found_page = B_FALSE;
			continue;
		}

		item = MP_CACHE_ITEM(cache, page, idx);
	}

	VALGRIND_MEMPOOL_ALLOC(mp_segment, item, num * cache->c_item_size);

#	ifdef MEMPOOL_TRACE
	if(mp_trace_slab) {
		logmsg(LOG_TRACE, "ALLOC SLAB %s %p @page: %p num %d idx: %d <- %p",
				cache->c_name, cache, page, num, idx, item);
	}
#	endif

	return item;
}

void mp_cache_free_array(mp_cache_t* cache, void* array, unsigned num) {
	mp_cache_page_t *page = NULL;
	int idx = 0;
	boolean_t found_page = B_FALSE;

	/* Align num */
	if(num > MP_BITMAP_BITS)
		num += MP_BITMAP_BITS - (num % MP_BITMAP_BITS);

	rwlock_lock_read(&cache->c_list_lock);
	list_for_each_entry(mp_cache_page_t, page, &cache->c_page_list, cp_node) {
		if(MP_CACHE_ITEM_IN_PAGE(page, array)) {
			found_page = B_TRUE;
			break;
		}
	}
	rwlock_unlock(&cache->c_list_lock);

	assert(found_page);

	idx = MP_CACHE_ITEM_INDEX(page, array);

	if(num <= MP_BITMAP_BITS) {
		mp_bitmap_free(page->cp_bitmap, num, idx);
	}
	else {
		mp_bitmap_free_cells(page->cp_bitmap, num / MP_BITMAP_BITS, idx);
	}

	atomic_add(&page->cp_free_items, num);

	VALGRIND_MEMPOOL_FREE(mp_segment, array);

#	ifdef MEMPOOL_TRACE
	if(mp_trace_slab) {
		logmsg(LOG_TRACE, "FREE SLAB %s %p @page: %p num: %d idx: %d <- %p",
				cache->c_name, cache, page, num, idx, array);
	}
#	endif
}

void* mp_cache_alloc(mp_cache_t* cache) {
	return mp_cache_alloc_array(cache, 1);
}

void mp_cache_free(mp_cache_t* cache, void* ptr) {
	mp_cache_free_array(cache, ptr, 1);
}

/**
 * Allocate at least sz bytes and return it from mempool allocators
 *
 * For large (> MPHEAPMAXALLOC) or aligned (sz & MPFRAGMASK == 0) allocations uses frag allocator
 * Otherwise allocates from mempool heap
 *
 * @note Never returns NULL. If all memory in mempool segment is exhausted, it will abort execution
 * */
void* mp_malloc(size_t sz) {
	void* ptr;

#	ifdef MEMPOOL_TRACE
	logmsg(LOG_TRACE, "ALLOC %zd", sz);
#	endif

	if((sz > MPHEAPMAXALLOC) || ((sz & MPFRAGMASK) == 0)) {
		ptr = mp_frag_alloc(sz, FRAG_COMMON);
	}
	else {
		ptr = mp_heap_alloc(sz);
	}

#	ifdef MEMPOOL_TRACE
	logmsg(LOG_TRACE, "ALLOC %p : %zd", ptr, sz);
#	endif

	return ptr;
}

size_t mp_get_size(void* ptr) {
	mp_frag_descr_t* descr = mp_frag_get_descr(ptr);

	assert(descr != NULL);

	if(descr->fd_type == FRAG_HEAP)
		return mp_heap_get_size(ptr);

	return mp_frag_get_size(descr);
}

/**
 * Reallocate memory at oldptr to size sz.
 *
 * Doesn't shrink memory
 * */
void* mp_realloc(void* oldptr, size_t sz) {
	void* newptr = mp_malloc(sz);
	size_t oldsz = mp_get_size(oldptr);

	if(oldsz >= sz)
		return oldptr;

	newptr = mp_malloc(sz);

	memcpy(newptr, oldptr, oldsz);

	mp_free(oldptr);

	return newptr;
}

void mp_free(void* ptr) {
	mp_frag_descr_t* descr = mp_frag_get_descr(ptr);

	assert(descr != NULL);

#	ifdef MEMPOOL_TRACE
	logmsg(LOG_TRACE, "FREE %p", ptr);
#	endif

	if(descr->fd_type == FRAG_HEAP)
		mp_heap_free(ptr);
	else
		mp_frag_free(ptr);

#	ifdef MEMPOOL_TRACE
	logmsg(LOG_TRACE, "FREE %p", ptr);
#	endif
}

int mempool_init(void) {
	/* FIXME: Add tuneables */

	mp_segment = plat_mp_seg_alloc(mp_segment_size);

	VALGRIND_CREATE_MEMPOOL(mp_segment, MEMPOOL_REDZONE_SIZE, B_FALSE);

	mp_frag_allocator_init();
	mp_heap_allocator_init();

	logmsg(LOG_DEBUG, "Allocated mempool segment @%p of size %lx", mp_segment, mp_segment_size);

	return 0;
}

void mempool_fini(void) {
	mp_heap_allocator_destroy();
	mp_frag_allocator_destroy();

	VALGRIND_DESTROY_MEMPOOL(mp_segment);

	plat_mp_seg_free(mp_segment, mp_segment_size);
}

#endif


