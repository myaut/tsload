/*
    This file is part of TSLoad.
    Copyright 2015, Sergey Klyaus, Tune-IT

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

#include <tsload/autostring.h>
#include <tsload/mempool.h>

#include <hostinfo/fsinfo.h>
#include <hostinfo/diskinfo.h>

#include <hitrace.h>

#include <windows.h>

#define HIWNETBUFLEN		(16 * SZ_KB)

/**
 * ### Windows
 * 
 * Local filesystems are added by DiskInfo code while iterating over volumes.
 * FSInfo adds network drives to them by using `WNetOpenEnum()` API call
 * 
 * Windows implementation doesn't support number of files 
 */

void hi_win_get_fssizes(const char* name, hi_fsinfo_t* fsi) {
	ULARGE_INTEGER free, total;
	DWORD spc, bps, freecl, totalcl;
	
	if(GetDiskFreeSpaceEx(name, NULL, &total, &free)) {
		fsi->fs_space = total.QuadPart;
		fsi->fs_space_free = free.QuadPart;
	}
	else {
		hi_fs_dprintf("hi_win_proc_vol_mntpts: GetDiskFreeSpaceEx('%s') error: %d\n", 
						name, GetLastError());
	}
	
	if(GetDiskFreeSpace(name, &spc, &bps, &freecl, &totalcl)) {
		fsi->fs_block_size = spc * bps;
		fsi->fs_frag_size = bps;
	}
	else {
		hi_fs_dprintf("hi_win_proc_vol_mntpts: GetDiskFreeSpace('%s') error: %d\n", 
						name, GetLastError());
	}
}

int hi_win_probe_netdisks(void) {
	/* On-disk filesystems are handled in diskinfo code -- here we 
	 * process Netword Disks using WNet API */
	HANDLE wnet_hdl;
	DWORD err;
	
	DWORD count;
	DWORD bufsz = HIWNETBUFLEN;
	int i;
	
	void* buffer = mp_malloc(bufsz);
	NETRESOURCE* netres = buffer;
	
	hi_fsinfo_t* fsi;
	
	err = WNetOpenEnum(RESOURCE_CONNECTED, RESOURCETYPE_DISK, 0, NULL, &wnet_hdl);
	if(err != NO_ERROR) {
		hi_fs_dprintf("hi_fsinfo_probe: WNetOpenEnum() ended with error: %d\n", err);
		return HI_PROBE_ERROR;
	}
	
	do {
		count = -1;
		
		err = WNetEnumResource(wnet_hdl, &count, buffer, &bufsz);
		if(err != NO_ERROR)
			break;
		
		for(i = 0; i < count; ++i) {
			fsi = hi_fsinfo_create(netres[i].lpLocalName, "CIFS", netres[i].lpRemoteName);
			
			hi_win_get_fssizes(netres[i].lpLocalName, fsi);
			
			/* Generic assumption based on MS documentation */
			fsi->fs_namemax = 255;
			/* Writeable access is deduced from ACLs, so we assume all 
			 * shares are writeable, but some of them may rise EPERM errors */
			fsi->fs_readonly = B_FALSE;
			
			hi_fsinfo_add(fsi);
			
			hi_fs_dprintf("hi_fsinfo_probe: Got resource local='%s' remote='%s' type=%x\n", 
						  netres[i].lpLocalName, netres[i].lpRemoteName, netres[i].dwDisplayType);
		}
	} while(err == NO_ERROR);
	
	if(err != ERROR_NO_MORE_ITEMS) {
		hi_fs_dprintf("hi_fsinfo_probe: WNetEnumResource() ended with error: %d\n", err);
	}
	
	WNetCloseEnum(wnet_hdl);
	
	return HI_PROBE_OK;
}

PLATAPI int hi_fsinfo_probe_impl(list_head_t* disks) {
	return hi_win_probe_netdisks();
}