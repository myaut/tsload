/*
 * mempool.c
 *
 *  Created on: 02.01.2013
 *      Author: myaut
 */

#include <mempool.h>

#include <stdlib.h>
#include <assert.h>

#include <sys/mman.h>

PLATAPI void* plat_mp_seg_alloc(size_t seg_size) {
	void* seg = mmap(NULL, seg_size, PROT_READ | PROT_WRITE,
					 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	assert(seg != NULL);

	mlock(seg, seg_size);

	return seg;
}

PLATAPI int plat_mp_seg_free(void* seg, size_t seg_size) {
	return munmap(seg, seg_size);
}
