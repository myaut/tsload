
/*
    This file is part of TSLoad.
    Copyright 2013-2014, Sergey Klyaus, ITMO University

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

#include <tsload/filemmap.h>

#include <windows.h>


PLATAPI int mmf_open(mmap_file_t* mmf, const char* filename, int mmfl) {
	OFSTRUCT of;

	mmf->mmf_mode = mmfl;
	mmf->mmf_file = OpenFile(filename, &of, mmfl);
	mmf->mmf_map = INVALID_HANDLE_VALUE;

	if(mmf->mmf_file == HFILE_ERROR)
		return MME_OPEN_ERROR;

	return MME_OK;
}

PLATAPI void mmf_close(mmap_file_t* mmf) {
	if(mmf->mmf_file != HFILE_ERROR)
		CloseHandle(mmf->mmf_file);
}

PLATAPI int mmf_create(mmap_file_t* mmf, long long offset, size_t length, void** mapping_ptr) {
	DWORD prot;
	DWORD file_size_high;
	DWORD file_size_low;
	void* ptr;

	switch(mmf->mmf_mode) {
	case MMFL_RDONLY:
		prot = FILE_MAP_READ;
		break;
	case MMFL_WRONLY:
		prot = FILE_MAP_WRITE;
		break;
	case MMFL_RDWR:
		prot = FILE_MAP_WRITE | FILE_MAP_READ;
		break;
	default:
		return MME_INVALID_FLAG;
	}

	file_size_low = GetFileSize((HANDLE) mmf->mmf_file, &file_size_high);

	/* Windows can't map zero-lenght files */
	if(file_size_low == 0 && file_size_high == 0)
		return MME_MMAP_ERROR;

	if(offset == MMF_MAP_ALL) {
		offset = 0;
		length = (((DWORD64) file_size_high) << 32) | file_size_low;
	}

	mmf->mmf_map = CreateFileMapping((HANDLE) mmf->mmf_file, NULL,
								     prot, file_size_high, file_size_low, NULL);

	if(mmf->mmf_map == NULL)
		return MME_MMAP_ERROR;

	ptr = MapViewOfFile(mmf->mmf_map, prot, offset & 0xFFFFFFFF,
						 offset >> 32, length);
	if(ptr == NULL)
		return MME_MMAP_ERROR;

	*mapping_ptr = ptr;
	return MME_OK;
}

PLATAPI void mmf_destroy(mmap_file_t* mmf, void* mapping) {
	UnmapViewOfFile(mapping);
	CloseHandle(mmf->mmf_map);
}

