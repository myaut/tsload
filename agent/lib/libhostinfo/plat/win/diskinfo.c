
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

#include <tsload/mempool.h>
#include <tsload/autostring.h>

#include <hostinfo/diskinfo.h>
#include <hostinfo/fsinfo.h>

#include <hitrace.h>

#include <stdlib.h>

#include <windows.h>
#include <initguid.h>
#include <devguid.h>
#include <setupapi.h>
#include <cfgmgr32.h>


/**
 * ### Windows
 *
 * Unlike traditional approach which goes from the top by enumerating drive letters from A: to Z:
 * or using generic `\\PhysicalDriveX\` device names, DiskInfo on Windows uses SetupDi APIs to
 * get PnP device ids. Than using ioctls it iterates its partitions. 
 * 
 * However, meaningful disk unit in Windows is Volume, so DiskInfo iterates over available volumes
 * using `FindFirstVolume()`/`FindNextVolume()`, collects volume extents and them binds volumes
 * to disk partitions according block numbers on which they reside. Also, it construct FSInfo 
 * objects from volume information.
 * 
 * __NOTE__: Currently `\\PhysicalDriveX\` device name isn't generated and DiskInfo use PNP
 * device name. This behavior may change in future versions of DiskInfo.
 * 
 * __NOTE__: Currently DiskInfo uses generic suffix `PartitionX` for partitions which is not useable 
 * in API calls. It is only used for reference.
 *
 * __References__:
 * * [Enumdisk1.exe: Enumdisk Sample for Enumerating Disk Devices](http://support.microsoft.com/kb/264203/en)
 * * [Enumerating and using partitions and volumes in Windows operating system](http://velisthoughts.blogspot.ru/2012/02/enumerating-and-using-partitions-and.html)
 */


/* TODO: PNPDeviceId -> PhysicalDisk and correct partition naming
 * */

#define HIWINVOLFSNAMELEN	32

#define HI_WIN_DSK_OK		0
#define HI_WIN_DSK_ERROR	1
#define HI_WIN_DSK_NODEV	2

#define HI_WIN_MAX_VOL_EXTENTS		256

list_head_t hi_win_vol_list;

/* from plat/win/fsinfo.c */
void hi_win_get_fssizes(const char* name, hi_fsinfo_t* fsi);

const char* hi_win_dsk_bus_type[BusTypeMax] = {
		"UNKNOWN", "SCSI", "ATAPI", "ATA", "1394",
		"SSA", "FC", "USB", "RAID", "iSCSI", "SAS",
		"SATA", "SSD", "VIRTUAL", "FB-VIRT"
};

/* Global state descriptor that is passed via 1st argument
 * After all operations are finished cleaned by hi_win_dsk_cleanup */
typedef struct hi_win_dsk {
	HDEVINFO dev_info;
	HDEVINFO int_dev_info;

	LONG index;

	LPTSTR dev_phys_buf;
	LPTSTR dev_loc_buf;
	LPTSTR dev_hid_buf;

	SP_DEVINFO_DATA dev_info_data;

	HANDLE disk_handle;

	SP_DEVICE_INTERFACE_DATA int_data;
	PSP_DEVICE_INTERFACE_DETAIL_DATA p_int_detail;

	char* adp_buf;
	char* dev_buf;
	char* geo_buf;
	char* layout_buf;

	PSTORAGE_ADAPTER_DESCRIPTOR adp_desc;
	PSTORAGE_DEVICE_DESCRIPTOR dev_desc;
} hi_win_dsk_t;

typedef struct hi_win_vol {
	AUTOSTRING char* vol_name;
	HANDLE vol_hdl;
	
	PVOLUME_DISK_EXTENTS extents;
	
	char name_ex[MAX_PATH];
	DWORD serial_no;
	
	DWORD fs_namemax;
	DWORD fs_flags;
	char fs_name[HIWINVOLFSNAMELEN];

#if HI_WIN_USE_STORAGE_READ_CAPACITY
	STORAGE_READ_CAPACITY capacity;
#else
	GET_LENGTH_INFORMATION len_info;
#endif

	hi_dsk_info_t* di;
	
	list_node_t node;
} hi_win_vol_t;

void hi_win_dsk_init(hi_win_dsk_t* dsk, LONG index) {
	dsk->index = index;

	dsk->p_int_detail = NULL;

	dsk->dev_hid_buf = NULL;
	dsk->dev_loc_buf = NULL;
	dsk->dev_phys_buf = NULL;

	dsk->adp_buf = NULL;
	dsk->dev_buf = NULL;
	dsk->geo_buf = NULL;
	dsk->layout_buf = NULL;

	dsk->disk_handle = INVALID_HANDLE_VALUE;
}

void hi_win_dsk_cleanup(hi_win_dsk_t* dsk) {
	if(dsk->p_int_detail)
		mp_free(dsk->p_int_detail);
	if(dsk->dev_loc_buf)
		mp_free(dsk->dev_hid_buf);
	if(dsk->dev_loc_buf)
		mp_free(dsk->dev_loc_buf);
	if(dsk->dev_phys_buf)
		mp_free(dsk->dev_phys_buf);

	if(dsk->disk_handle != INVALID_HANDLE_VALUE)
		CloseHandle(dsk->disk_handle);

	if(dsk->adp_buf)
		mp_free(dsk->adp_buf);
	if(dsk->dev_buf)
		mp_free(dsk->dev_buf);
	if(dsk->geo_buf)
		mp_free(dsk->geo_buf);
	if(dsk->layout_buf)
		mp_free(dsk->layout_buf);
}

/*
 * Wrapper for DeviceIoControl that allocates buffer if needed
 *
 * If out_buf_len == 0, hi_win_disk_ioctl uses temporary storage
 * as output buffer, allocates output buffer and stores pointer
 * to p_out_buf then copies data there. If out_buf_len > 0,
 * buffer from p_out_buf used directly.
 *
 * @param dsk 			state descriptor
 * @param request		IOCTL_... request id
 * @param in_buf		input buffer (may be NULL)
 * @param in_buf_length input buffer length
 * @param p_out_buf		pointer to pointer to output buffer
 * @param out_buf_len   output buffer length
 * */
int hi_win_disk_ioctl(hi_win_dsk_t* dsk, DWORD request, void* in_buf, size_t in_buf_len,
					  char** p_out_buf, unsigned out_buf_len) {
	BOOL status;
	char tmp_buf[512];
	char* out_buf;
	DWORD ret_len;

	/* Select output storage */
	if(out_buf_len == 0) {
		out_buf_len = 512;
		out_buf = tmp_buf;
	}
	else {
		out_buf = *p_out_buf;
	}

	hi_dsk_dprintf("hi_win_disk_ioctl: request: %d in: %p;%d out: %p:%d\n", request, in_buf,
						in_buf_len, out_buf, out_buf_len);

	status = DeviceIoControl(dsk->disk_handle, request,
							 in_buf, in_buf_len, out_buf, out_buf_len, &ret_len, NULL);

	if(status == FALSE)
		return HI_WIN_DSK_ERROR;

	if(*p_out_buf == NULL) {
		/* Allocate output buffer */
		*p_out_buf = mp_malloc(ret_len);
		memcpy(*p_out_buf, tmp_buf, ret_len);
	}

	return HI_WIN_DSK_OK;
}

void hi_win_vol_destroy(hi_win_vol_t* vol) {
	if(vol->extents)
		mp_free(vol->extents);
	
	aas_free(&vol->vol_name);
	mp_free(vol);
}

/* NOTE: on error frees all resources */
boolean_t hi_win_vol_ioctl_impl(hi_win_vol_t* vol, DWORD request, void* out_buf, 
								size_t out_buf_sz, const char* rq_name) {
	if(!DeviceIoControl(vol->vol_hdl, request, NULL, 0, out_buf, 
						out_buf_sz, &out_buf_sz, NULL)) {
		CloseHandle(vol->vol_hdl);
		hi_dsk_dprintf("hi_win_proc_volume: failed DeviceIoControl(%s, %s): code %d\n", 
						vol->vol_name, rq_name, GetLastError());
		hi_win_vol_destroy(vol);
		
		return HI_WIN_DSK_ERROR;
	}
	
	return HI_WIN_DSK_OK;
}
#define hi_win_vol_ioctl(vol, request, out_buf, out_buf_sz)			\
		hi_win_vol_ioctl_impl(vol, request, out_buf, out_buf_sz, #request)

int hi_win_proc_vol_mntpts(hi_win_vol_t* vol) {
	char* names = NULL;
	char* name = NULL;
	DWORD len = MAX_PATH + 1;
	
	hi_fsinfo_t* fsi;
	
	BOOL ok = FALSE;
	
	while(B_TRUE) {
		names = mp_malloc(len);
		
		if(!names)
			return HI_PROBE_ERROR;
		
		ok = GetVolumePathNamesForVolumeName(vol->di->d_disk_name, names, len, &len);
		if(ok)
			break;
		
		hi_fs_dprintf("hi_win_proc_vol_mntpts: GetVolumePathNamesForVolumeName('%s') error: %d\n", 
					  vol->di->d_disk_name, GetLastError());
		if(GetLastError() != ERROR_MORE_DATA)
			break;
		
		mp_free(names);
		names = NULL;
	}
	
	if(ok) {
		for(name = names; *name != '\0'; name += strlen(name) + 1) {
			fsi = hi_fsinfo_create(name, vol->fs_name, vol->di->d_disk_name);
		
			fsi->fs_device = vol->di;
			
			fsi->fs_readonly = TO_BOOLEAN(vol->fs_flags & FILE_READ_ONLY_VOLUME);
			fsi->fs_namemax = vol->fs_namemax;
			
			hi_win_get_fssizes(name, fsi);
			
			hi_fsinfo_add(fsi);
			
			hi_fs_dprintf("hi_win_proc_vol_mntpts: found mntpt '%s' for volume '%s'\n", 
						  name, vol->di->d_disk_name);
		}
	}
	
	mp_free(names);
	
	return HI_WIN_DSK_OK;
}

int hi_win_proc_volume(char* vol_name) {
	hi_win_vol_t* vol;
	HANDLE vol_hdl;
	
	size_t backslash_pos = strlen(vol_name) - 1;
	
	DWORD ioctl_sz;
	int error;
	
	LONGLONG vollength;

#ifdef HOSTINFO_TRACE	
	int extentid;
	PDISK_EXTENT extent;
#endif
	
	/* Remove vol_name trailing backslash */
	vol_name[backslash_pos] = '\0';
	vol_hdl = CreateFile(vol_name, GENERIC_READ, 
			 			 FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, 
					     OPEN_EXISTING, 0, 0);
	
	if(vol_hdl == INVALID_HANDLE_VALUE) {
		hi_dsk_dprintf("hi_win_proc_volume: failed CreateFile(%s): code %d\n", 
					   vol_name, GetLastError());
		return HI_WIN_DSK_ERROR;
	}
	
	/* Initialize vol object (but add it to list later) */
	vol = mp_malloc(sizeof(hi_win_vol_t));
	aas_copy(aas_init(&vol->vol_name), vol_name);
	
	vol->vol_hdl = vol_hdl;
	
	/* Get extents and size via ioctls() */
	ioctl_sz = sizeof(VOLUME_DISK_EXTENTS) + HI_WIN_MAX_VOL_EXTENTS * sizeof(DISK_EXTENT);
	vol->extents = mp_malloc(ioctl_sz);
	
	error = hi_win_vol_ioctl(vol, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 
							 vol->extents, ioctl_sz);
	if(error != HI_WIN_DSK_OK)
		return error;

#ifdef HI_WIN_USE_STORAGE_READ_CAPACITY
	ioctl_sz = vol->capacity.Version = sizeof(STORAGE_READ_CAPACITY);
	
	error = hi_win_vol_ioctl(vol, IOCTL_STORAGE_READ_CAPACITY, 
							 &vol->capacity, ioctl_sz);
	if(error != HI_WIN_DSK_OK)
		return error;

	vollength = vol->capacity.DiskLength.QuadPart;
#else
	ioctl_sz = sizeof(STORAGE_READ_CAPACITY);
	
	error = hi_win_vol_ioctl(vol, IOCTL_DISK_GET_LENGTH_INFO, 
							 &vol->len_info, ioctl_sz);
	if(error != HI_WIN_DSK_OK)
		return error;
	
	vollength = vol->len_info.Length.QuadPart;
#endif
	
	CloseHandle(vol_hdl);
	
	/* Get information on volume */
	vol_name[backslash_pos] = '\\';
	if(!GetVolumeInformation(vol_name, vol->name_ex, MAX_PATH,
							 &vol->serial_no, &vol->fs_namemax, &vol->fs_flags, 
						     vol->fs_name, HIWINVOLFSNAMELEN)) {
		if(GetLastError() != ERROR_UNRECOGNIZED_VOLUME) {
			hi_dsk_dprintf("hi_win_proc_volume: failed GetVolumeInformation(%s): code %d\n", 
						vol_name, GetLastError());
			return HI_WIN_DSK_ERROR;
		}
		
		/* RAW filesystem */
		vol->serial_no = 0;
		vol->fs_namemax = 0;
		vol->fs_flags = 0;
		strncpy(vol->name_ex, vol_name, MAX_PATH);
		strncpy(vol->fs_name, "RAW", HIWINVOLFSNAMELEN);
	}
	
	hi_dsk_dprintf("hi_win_proc_volume: find volume: %s, fs type: %s, size %lld\n", 
				   vol->name_ex, vol->fs_name, vollength);
	
	/* Create hi_disk_info object and add it to global list */
	vol->di = hi_dsk_create();
	
	aas_copy(&vol->di->d_disk_name, vol_name);
	aas_copy(&vol->di->d_id, vol->name_ex);
	
	/* Again, make it compatible with CreateFile() */
	vol_name[backslash_pos] = '\0';	
	aas_copy(&vol->di->d_path, vol_name);
	
	vol->di->d_size = vollength;
	vol->di->d_type = HI_DSKT_VOLUME;
	
	hi_dsk_add(vol->di);
	
	hi_win_proc_vol_mntpts(vol);
	
#ifdef HOSTINFO_TRACE
	for(extentid = 0; extentid < vol->extents->NumberOfDiskExtents; ++extentid) {
		extent = &(vol->extents->Extents[extentid]);
		
		hi_dsk_dprintf("hi_win_proc_volume: extent disk=%d offset=%lld size=%lld\n", 
				       (int) extent->DiskNumber, extent->StartingOffset.QuadPart, 
					   extent->ExtentLength.QuadPart);
	}
#endif

	list_add_tail(&vol->node, &hi_win_vol_list);
	
	return HI_WIN_DSK_OK;
}

int hi_win_proc_volumes(void) {
	HANDLE vol_find_hdl;
	char vol_name[MAX_PATH + 1];
	boolean_t success;
	
	vol_find_hdl = FindFirstVolume(vol_name, MAX_PATH);
	success = vol_find_hdl != INVALID_HANDLE_VALUE;
	
	if(!success) {
		hi_dsk_dprintf("hi_win_proc_volumes: FindFirstVolume() returned error: code %d\n", GetLastError());
		return HI_PROBE_ERROR;
	}
	
	while(success) {
		hi_dsk_dprintf("hi_win_proc_volumes: found volume %s\n", vol_name);
		hi_win_proc_volume(vol_name);
		
		success = FindNextVolume(vol_find_hdl, vol_name, MAX_PATH);
	}
	
	FindVolumeClose(vol_find_hdl);
	
	return HI_PROBE_OK;
}

void hi_win_destroy_volumes(void) {
	hi_win_vol_t* vol;
	hi_win_vol_t* next;
	
	list_for_each_entry_safe(hi_win_vol_t, vol, next, &hi_win_vol_list, node) {
		hi_win_vol_destroy(vol);
	}
}

void hi_win_bind_part_vol(hi_win_dsk_t* dsk, hi_dsk_info_t* partdi, LONGLONG part_start, LONGLONG part_size) {
	hi_win_vol_t* vol;
	
	int extentid;
	PDISK_EXTENT extent;
	
	LONGLONG ext_start, ext_end;
	LONGLONG part_end = part_start + part_size;
	
	hi_dsk_dprintf("hi_win_bind_part_vol: partition disk=%ld offset=%lld size=%lld\n", 
				   dsk->index, part_start, part_size);
	
	list_for_each_entry(hi_win_vol_t, vol, &hi_win_vol_list, node) {
		for(extentid = 0; extentid < vol->extents->NumberOfDiskExtents; ++extentid) {
			extent = &(vol->extents->Extents[extentid]);
			
			ext_start = extent->StartingOffset.QuadPart;
			ext_end = extent->ExtentLength.QuadPart;
			
			if(extent->DiskNumber == dsk->index && (ext_start >= part_start) && (ext_end <= part_end)) {
				hi_dsk_attach(partdi, vol->di);
				return;
			}
		}
	}
}

/*
 * Process partitions
 * */
int hi_win_proc_partitions(hi_win_dsk_t* dsk, hi_dsk_info_t* parent) {
	int part_id;
	int error;

	hi_dsk_info_t* di;

	unsigned layout_size = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + 127 * sizeof(PARTITION_INFORMATION_EX);
	PDRIVE_LAYOUT_INFORMATION_EX layout;
	PARTITION_INFORMATION_EX* partition;

	/* Pre-allocate layout buffer */
	dsk->layout_buf = mp_malloc(layout_size);

	error = hi_win_disk_ioctl(dsk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, &dsk->layout_buf, layout_size);
	if(error != HI_WIN_DSK_OK) {
		hi_dsk_dprintf("hi_win_proc_partitions: IOCTL_DISK_GET_DRIVE_LAYOUT_EX failed! code: %d\n", GetLastError());
		return error;
	}

	layout = (PDRIVE_LAYOUT_INFORMATION_EX) dsk->layout_buf;

	hi_dsk_dprintf("hi_win_proc_partitions: found %d partitions\n", layout->PartitionCount);

	for(part_id = 0; part_id < layout->PartitionCount; ++part_id) {
		partition = &layout->PartitionEntry[part_id];

		if(partition->PartitionNumber < 1)
			continue;

		di = hi_dsk_create();

		/* XXX: This is wrong */
		aas_printf(&di->d_hdr.name, "%s\\Partition%d", dsk->dev_phys_buf, partition->PartitionNumber);
		aas_copy(&di->d_path, di->d_hdr.name);

		di->d_size = partition->PartitionLength.QuadPart;
		di->d_type = HI_DSKT_PARTITION;

		hi_dsk_add(di);
		hi_dsk_attach(di, parent);
		
		hi_win_bind_part_vol(dsk, di, partition->StartingOffset.QuadPart, 
							 partition->PartitionLength.QuadPart);
	}

	return HI_WIN_DSK_OK;
}

int hi_win_proc_disk(hi_win_dsk_t* dsk) {
	STORAGE_PROPERTY_QUERY query;
	int error;

	PSTORAGE_ADAPTER_DESCRIPTOR  adapter;
	PSTORAGE_DEVICE_DESCRIPTOR device;
	PDISK_GEOMETRY_EX geometry;

	char* vendor_id = "";
	char* product_id = "";

	hi_dsk_info_t* di;

	dsk->disk_handle = CreateFile(dsk->p_int_detail->DevicePath,
								  GENERIC_READ | GENERIC_WRITE,
								  FILE_SHARE_READ | FILE_SHARE_WRITE,
								  NULL, OPEN_EXISTING, 0, NULL);

	if(dsk->disk_handle == INVALID_HANDLE_VALUE)
		return HI_WIN_DSK_ERROR;

	query.PropertyId = StorageAdapterProperty;
	query.QueryType = PropertyStandardQuery;
	error = hi_win_disk_ioctl(dsk, IOCTL_STORAGE_QUERY_PROPERTY, &query,
							  sizeof(STORAGE_PROPERTY_QUERY), &dsk->adp_buf, 0);

	if(error != HI_WIN_DSK_OK)
		return error;

	query.PropertyId = StorageDeviceProperty;
	query.QueryType = PropertyStandardQuery;
	error = hi_win_disk_ioctl(dsk, IOCTL_STORAGE_QUERY_PROPERTY, &query,
							  sizeof(STORAGE_PROPERTY_QUERY), &dsk->dev_buf, 0);

	if(error != HI_WIN_DSK_OK)
		return error;

	error = hi_win_disk_ioctl(dsk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &dsk->geo_buf, 0);

	if(error != HI_WIN_DSK_OK)
		return error;

	adapter = (PSTORAGE_ADAPTER_DESCRIPTOR) dsk->adp_buf;
	device = (PSTORAGE_DEVICE_DESCRIPTOR) dsk->dev_buf;
	geometry = (PDISK_GEOMETRY_EX) dsk->geo_buf;

	/* Creating a disk */
	di = hi_dsk_create();
	aas_copy(&di->d_hdr.name, dsk->dev_phys_buf);
	aas_copy(&di->d_path, dsk->p_int_detail->DevicePath);

	di->d_size = geometry->DiskSize.QuadPart;
	di->d_type = HI_DSKT_DISK;

	if(adapter->BusType < BusTypeMax)
		aas_copy(&di->d_bus_type, hi_win_dsk_bus_type[adapter->BusType]);

	if(dsk->dev_loc_buf)
		aas_copy(&di->d_port, dsk->dev_loc_buf);
	if(dsk->dev_hid_buf)
		aas_copy(&di->d_id, dsk->dev_hid_buf);

	if(device->VendorIdOffset > 0)
		vendor_id = &dsk->dev_buf[device->VendorIdOffset];
	if(device->ProductIdOffset > 0)
		product_id = &dsk->dev_buf[device->ProductIdOffset];

	aas_printf(&di->d_model, "%s %s", vendor_id, product_id);

	hi_dsk_add(di);

	hi_win_proc_partitions(dsk, di);

	return HI_WIN_DSK_OK;
}

/*
 * Wrapper for SetupDiGetDeviceRegistryProperty.
 *
 * During first pass it's gets desired buffer size for property,
 * then allocates buffer and reads property.
 * */
int hi_win_get_reg_property(hi_win_dsk_t* dsk, DWORD property, char** buf) {
	DWORD error_code;
	BOOL status;
	DWORD buf_size = 0;
	DWORD data_type;

	/* Get buffer size */
	status = SetupDiGetDeviceRegistryProperty(dsk->dev_info, &dsk->dev_info_data,
			property, &data_type, (PBYTE)*buf, buf_size, &buf_size);
	if ( status == FALSE ) {
		error_code = GetLastError();
		if ( error_code != ERROR_INSUFFICIENT_BUFFER ) {
			return HI_WIN_DSK_ERROR;
		}
	}

	*buf = mp_malloc(buf_size);

	/* Get property */
	status = SetupDiGetDeviceRegistryProperty(dsk->dev_info, &dsk->dev_info_data,
					property, &data_type, (PBYTE)*buf, buf_size, &buf_size);

	if(status == FALSE)
		return HI_WIN_DSK_ERROR;

	return HI_WIN_DSK_OK;
}

/*
 * Process device */
int hi_win_proc_dev(hi_win_dsk_t* dsk) {
	DWORD error_code;
	BOOL status;

	int error;

	dsk->dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
	status = SetupDiEnumDeviceInfo(dsk->dev_info, dsk->index, &dsk->dev_info_data);

	if(status == FALSE) {
		error_code = GetLastError();
		hi_dsk_dprintf("hi_win_proc_dev: SetupDiEnumDeviceInfo failed: 0x%lx\n", error_code);
		if(error_code == ERROR_NO_MORE_ITEMS)
			return HI_WIN_DSK_NODEV;
		return HI_WIN_DSK_ERROR;
	}

	/* Hardware ID and Location Information are optional */
	error = hi_win_get_reg_property(dsk, SPDRP_LOCATION_INFORMATION, &dsk->dev_loc_buf);
	error = hi_win_get_reg_property(dsk, SPDRP_HARDWAREID, &dsk->dev_hid_buf);

	error = hi_win_get_reg_property(dsk, SPDRP_PHYSICAL_DEVICE_OBJECT_NAME, &dsk->dev_phys_buf);
	if(error != HI_WIN_DSK_OK) {
		hi_dsk_dprintf("hi_win_proc_dev: get SPDRP_PHYSICAL_DEVICE_OBJECT_NAME failed: %d\n", error);
		return error;
	}

	hi_dsk_dprintf("hi_win_proc_dev: loc: %s phys: %s\n", dsk->dev_loc_buf, dsk->dev_phys_buf);

	return HI_WIN_DSK_OK;
}

/*
 * Process device interface
 *
 * Collects PnP device path here*/
int hi_win_proc_int(hi_win_dsk_t* dsk) {
	DWORD req_size, int_detail_size;
	DWORD error_code;
	BOOL status;

	dsk->int_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	status = SetupDiEnumDeviceInterfaces(dsk->int_dev_info, 0, (LPGUID) &DiskClassGuid, dsk->index, &dsk->int_data);
	if(status == FALSE) {
		error_code = GetLastError();
		if(error_code == ERROR_NO_MORE_ITEMS)
			return HI_WIN_DSK_NODEV;
		hi_dsk_dprintf("hi_win_proc_int: SetupDiEnumDeviceInterfaces failed\n");
		return HI_WIN_DSK_ERROR;
	}

	/* Get required size fo detail data */
	status = SetupDiGetDeviceInterfaceDetail(dsk->int_dev_info, &dsk->int_data, NULL, 0, &req_size, NULL);
	if(status == FALSE) {
		error_code = GetLastError();
		hi_dsk_dprintf("hi_win_proc_int: 1st SetupDiGetDeviceInterfaceDetail failed: %lx\n", error_code);
		if(error_code != ERROR_INSUFFICIENT_BUFFER)
			return HI_WIN_DSK_ERROR;
	}

	int_detail_size = req_size;
	dsk->p_int_detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA) mp_malloc(req_size);
	dsk->p_int_detail->cbSize = sizeof (SP_INTERFACE_DEVICE_DETAIL_DATA);

	status = SetupDiGetDeviceInterfaceDetail(dsk->int_dev_info, &dsk->int_data, dsk->p_int_detail, int_detail_size, &req_size, NULL);

	if(status == FALSE) {
		hi_dsk_dprintf("hi_win_proc_int: 2nd SetupDiGetDeviceInterfaceDetail failed: %lx\n", error_code);
		return HI_WIN_DSK_ERROR;
	}

	hi_dsk_dprintf("hi_win_proc_int: found device interface: %s\n", dsk->p_int_detail->DevicePath);

	return hi_win_proc_disk(dsk);
}

int hi_dsk_probe_disks(void) {
	HDEVINFO dev_info, int_dev_info;
	LONG index = 0;
	BOOL status;

	int error = HI_WIN_DSK_OK;

	hi_win_dsk_t dsk;

	dev_info = SetupDiGetClassDevs((LPGUID) &GUID_DEVCLASS_DISKDRIVE, NULL, NULL, DIGCF_PRESENT);

	if(dev_info == INVALID_HANDLE_VALUE) {
		return HI_PROBE_ERROR;
	}

	int_dev_info = SetupDiGetClassDevs((LPGUID) &DiskClassGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);

	if(int_dev_info == INVALID_HANDLE_VALUE) {
		SetupDiDestroyDeviceInfoList(dev_info);
		return HI_PROBE_ERROR;
	}

	dsk.dev_info = dev_info;
	dsk.int_dev_info = int_dev_info;

	hi_dsk_dprintf("hi_dsk_probe: Got dev_info %x and int_dev_info %x\n", (DWORD) dev_info, (DWORD) int_dev_info);

	while(error != HI_WIN_DSK_NODEV) {
		hi_win_dsk_init(&dsk, index);

		error = hi_win_proc_dev(&dsk);

		if(error == HI_WIN_DSK_OK) {
			error = hi_win_proc_int(&dsk);
		}

		hi_dsk_dprintf("hi_dsk_probe: probed device #%d error: %d system error: %d\n", index, error, GetLastError());

		hi_win_dsk_cleanup(&dsk);

		++index;
	}

	SetupDiDestroyDeviceInfoList(dev_info);
	SetupDiDestroyDeviceInfoList(int_dev_info);

	return HI_PROBE_OK;
}

PLATAPI int hi_dsk_probe(void) {
	int error = HI_PROBE_OK;
	
	list_head_init(&hi_win_vol_list, "win-vol-list");
	
	error = hi_win_proc_volumes();	
	if(error != HI_PROBE_OK)
		goto end;
	
	error = hi_dsk_probe_disks();		

end:	
	hi_win_destroy_volumes();	
	return error;
}

