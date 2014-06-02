
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

