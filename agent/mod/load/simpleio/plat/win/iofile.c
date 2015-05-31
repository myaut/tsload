
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



#include <tsload/defs.h>

#include <simpleio.h>
#include <mod/simpleio/plat/iofile.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <windows.h>

PLATAPI int io_file_init(io_file_t* iof, io_file_type_t type, const char* path, uint64_t file_size) {
	iof->iof_impl.hdl = INVALID_HANDLE_VALUE;
	
	aas_copy(aas_init(&iof->iof_path), path);
	
	iof->iof_exists = B_FALSE;
	iof->iof_file_size = file_size;
	iof->iof_file_type = type;
	
	aas_init(&iof->iof_error_msg);
	
	return 0;
}

PLATAPI int io_file_stat(io_file_t* iof)
{
    /* From http://stackoverflow.com/questions/8991192/check-filesize-without-opening-file-in-c */
    WIN32_FILE_ATTRIBUTE_DATA fad;
	LARGE_INTEGER size;
	
	/* Assume that file exists, but if GetFileAttributesEx fails, it doesn't */
	iof->iof_exists = B_TRUE;
	
    if (!GetFileAttributesEx(iof->iof_path, GetFileExInfoStandard, &fad)) {
		DWORD error = GetLastError();
		
		if(error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
			iof->iof_exists = B_FALSE;
			return 0;
		}
		
		aas_printf(&iof->iof_error_msg, "Failed to GetFileAttributesEx('%s'): error %d", 
				   iof->iof_path, error);
		
        return -1;
	}
    
    size.HighPart = fad.nFileSizeHigh;
    size.LowPart = fad.nFileSizeLow;
    iof->iof_file_size = (uint64_t) size.QuadPart;
	
	return 0;
}

PLATAPI int io_file_open(io_file_t* iof, boolean_t rdwr, boolean_t sync) {
	DWORD access = GENERIC_READ | ((rdwr) ? GENERIC_WRITE : 0);
	DWORD share = FILE_SHARE_READ | 
				  ((rdwr || iof->iof_file_type == IOF_BLOCKDEV) ? FILE_SHARE_WRITE : 0);
	DWORD flags = (sync)?  FILE_FLAG_WRITE_THROUGH : 0;
	DWORD creat = (iof->iof_exists || iof->iof_file_type == IOF_BLOCKDEV) 
						? OPEN_EXISTING : CREATE_NEW;
	
	HANDLE hdl;
	
	hdl = CreateFile(iof->iof_path, access, share, NULL, creat, flags, NULL);

	if(hdl == INVALID_HANDLE_VALUE && iof->iof_error_msg == NULL) {
		aas_printf(&iof->iof_error_msg, 
				   "Failed to CreateFile('%s', 0x%lx, 0x%lx, NULL, 0x%x, 0x%x): error %d", 
				   iof->iof_path, access, share, creat, flags, GetLastError());
		return -1;
	}
	
	iof->iof_impl.hdl = hdl;

	return 0;
}

PLATAPI void io_file_close(io_file_t* iof, boolean_t do_remove) {
	if(iof->iof_impl.hdl != INVALID_HANDLE_VALUE) {
		CloseHandle(iof->iof_impl.hdl);

		if(do_remove) {
			DeleteFile(iof->iof_path);
		}
		
		iof->iof_impl.hdl = INVALID_HANDLE_VALUE;
	}
	
	aas_free(&iof->iof_path);
	aas_free(&iof->iof_error_msg);
}

PLATAPI int io_file_fsync(io_file_t* iof) {
	BOOL ret = FlushFileBuffers(iof->iof_impl.hdl);
	
	if(!ret && iof->iof_error_msg == NULL) {
		aas_printf(&iof->iof_error_msg, "Failed to FlushFileBuffers('%s'): error %d", 
				   iof->iof_path, GetLastError());
	}
	
	return ret;
}

PLATAPI int io_file_seek(io_file_t* iof, uint64_t where) {
	LONG low = where & 0xFFFFFFFF;
	LONG high = where >> 32;
	
	DWORD plow = SetFilePointer(iof->iof_impl.hdl, low, &high, FILE_BEGIN);
	
	if(plow == INVALID_SET_FILE_POINTER && iof->iof_error_msg == NULL) {
		aas_printf(&iof->iof_error_msg, "Failed to SetFilePointer('%s', %" PRId64 "): %s", 
				   iof->iof_path, where, GetLastError());
		return -1;
	}
	
	return 0;
}

PLATAPI int io_file_pread(io_file_t* iof, void* buffer, size_t count, uint64_t offset) {
	DWORD nbytes;
	OVERLAPPED overlapped;
	
	BOOL ret;
	
	overlapped.Offset = offset & 0xFFFFFFFF;
	overlapped.OffsetHigh = offset >> 32;
	overlapped.hEvent = (HANDLE) 0;
	
	ret = ReadFile(iof->iof_impl.hdl, buffer, (DWORD) count, &nbytes, &overlapped);
	
	if(!ret)
		return -1;
	
	return (int) nbytes;
}

PLATAPI int io_file_pwrite(io_file_t* iof, void* buffer, size_t count, uint64_t offset) {
	DWORD nbytes;
	OVERLAPPED overlapped;
	
	BOOL ret;
	
	overlapped.Offset = offset & 0xFFFFFFFF;
	overlapped.OffsetHigh = offset >> 32;
	overlapped.hEvent = (HANDLE) 0;
	
	ret = WriteFile(iof->iof_impl.hdl, buffer, (DWORD) count, &nbytes, &overlapped);
	
	if(!ret)
		return -1;
	
	return (int) nbytes;
}

