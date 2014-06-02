
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

