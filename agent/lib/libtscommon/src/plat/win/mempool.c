/*
 * mempool.c
 *
 *  Created on: 02.01.2013
 *      Author: myaut
 */

#include <mempool.h>

#include <stdlib.h>
#include <assert.h>

#include <windows.h>

PLATAPI void* plat_mp_seg_alloc(size_t seg_size) {
	void* seg = VirtualAlloc(NULL, seg_size,
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	assert(seg != NULL);

	VirtualLock(seg, seg_size);

	return seg;
}

PLATAPI int plat_mp_seg_free(void* seg, size_t seg_size) {
	return VirtualFree(seg, seg_size, MEM_RELEASE);
}
