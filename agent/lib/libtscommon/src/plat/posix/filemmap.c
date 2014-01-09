/*
 * filemmap.c
 *
 *  Created on: 09.01.2013
 *      Author: myaut
 */

#include <filemmap.h>

#include <sys/mman.h>
#include <unistd.h>

PLATAPI int mmf_open(mmap_file_t* mmf, const char* filename, int mmfl) {
	mmf->mmf_mode = mmfl;
	mmf->mmf_fd = open(filename, mmfl);

	if(mmf->mmf_fd == -1)
		return MME_OPEN_ERROR;

	return MME_OK;
}

PLATAPI void mmf_close(mmap_file_t* mmf) {
	if(mmf->mmf_fd != -1)
		close(mmf->mmf_fd);
}

PLATAPI int mmf_create(mmap_file_t* mmf, long long offset, size_t length, void** mapping_ptr) {
	int prot;
	void* area;

	switch(mmf->mmf_mode) {
	case MMFL_RDONLY:
		prot = PROT_READ;
		break;
	case MMFL_WRONLY:
		prot = PROT_WRITE;
		break;
	case MMFL_RDWR:
		prot = PROT_READ | PROT_WRITE;
		break;
	default:
		return MME_INVALID_FLAG;
	}

	if(offset == MMF_MAP_ALL) {
		offset = 0;
		length = lseek(mmf->mmf_fd, 0, SEEK_END);
	}

	mmf->mmf_length = length;
	area = mmap(NULL, length, prot, MAP_PRIVATE, mmf->mmf_fd, offset);

	if(area == MAP_FAILED) {
		return MME_MMAP_ERROR;
	}

	*mapping_ptr = area;
	return MME_OK;
}

PLATAPI void mmf_destroy(mmap_file_t* mmf, void* mapping) {
	munmap(mapping, mmf->mmf_length);
}
