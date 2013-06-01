/*
 * diskinfo.c
 *
 *  Created on: 28.04.2013
 *      Author: myaut
 */

#include <diskinfo.h>
#include <mempool.h>

#include <stdlib.h>

#include <windows.h>
#include <initguid.h>
#include <devguid.h>
#include <setupapi.h>   // for SetupDiXxx functions.
#include <cfgmgr32.h>   // for SetupDiXxx functions.

/**
 * diskinfo (Windows)
 *
 * Uses WinAPI and Driver's Setup API to enumerate disks/partitions
 *
 * @see http://support.microsoft.com/kb/264203
 *
 * TODO: PNPDeviceId -> PhysicalDisk and correct partition naming
 * TODO: Volumes
 * */

#define HI_WIN_DSK_OK		0
#define HI_WIN_DSK_ERROR	1
#define HI_WIN_DSK_NODEV	2

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

/**
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

/**
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
		snprintf(di->d_name, HIDSKNAMELEN, "%s\\Partition%d", dsk->dev_phys_buf, partition->PartitionNumber);
		strncpy(di->d_path, di->d_name, HIDSKPATHLEN);

		di->d_size = partition->PartitionLength.QuadPart;
		di->d_type = HI_DSKT_PARTITION;

		hi_dsk_add(di);
		hi_dsk_attach(di, parent);
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
	strncpy(di->d_name, dsk->dev_phys_buf, HIDSKNAMELEN);
	strncpy(di->d_path, dsk->p_int_detail->DevicePath, HIDSKPATHLEN);

	di->d_size = geometry->DiskSize.QuadPart;
	di->d_type = HI_DSKT_DISK;

	if(adapter->BusType < BusTypeMax)
		strncpy(di->d_bus_type, hi_win_dsk_bus_type[adapter->BusType], HIDSKBUSLEN);

	if(dsk->dev_loc_buf)
		strncpy(di->d_port, dsk->dev_loc_buf, HIDSKPATHLEN);
	if(dsk->dev_hid_buf)
		strncpy(di->d_id, dsk->dev_hid_buf, HIDSKIDLEN);

	if(device->VendorIdOffset > 0)
		vendor_id = &dsk->dev_buf[device->VendorIdOffset];
	if(device->ProductIdOffset > 0)
		product_id = &dsk->dev_buf[device->ProductIdOffset];

	snprintf(di->d_model, HIDSKMODELLEN, "%s %s", vendor_id, product_id);

	hi_dsk_add(di);

	hi_win_proc_partitions(dsk, di);

	return HI_WIN_DSK_OK;
}

/**
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

/**
 * Process device */
int hi_win_proc_dev(hi_win_dsk_t* dsk) {
	DWORD error_code;
	BOOL status;

	int error;

	dsk->dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
	status = SetupDiEnumDeviceInfo(dsk->dev_info, dsk->index, &dsk->dev_info_data);

	if(status == FALSE) {
		error_code = GetLastError();
		hi_dsk_dprintf("hi_win_proc_dev: SetupDiEnumDeviceInfo failed\n");
		if(error_code == ERROR_NO_MORE_ITEMS)
			return HI_WIN_DSK_NODEV;
		return HI_WIN_DSK_ERROR;
	}

	/* Hardware ID and Location Information are optional */
	error = hi_win_get_reg_property(dsk, SPDRP_LOCATION_INFORMATION, &dsk->dev_loc_buf);
	error = hi_win_get_reg_property(dsk, SPDRP_HARDWAREID, &dsk->dev_hid_buf);

	error = hi_win_get_reg_property(dsk, SPDRP_PHYSICAL_DEVICE_OBJECT_NAME, &dsk->dev_phys_buf);
	if(error != HI_WIN_DSK_OK) {
		hi_dsk_dprintf("hi_win_proc_dev: get SPDRP_PHYSICAL_DEVICE_OBJECT_NAME failed\n");
		return error;
	}

	hi_dsk_dprintf("hi_win_proc_dev: loc: %s phys: %s\n", dsk->dev_loc_buf, dsk->dev_phys_buf);

	return HI_WIN_DSK_OK;
}

/**
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
		hi_dsk_dprintf("hi_win_proc_int: 1st SetupDiGetDeviceInterfaceDetail failed\n");
		if(error_code != ERROR_INSUFFICIENT_BUFFER)
			return HI_WIN_DSK_ERROR;
	}

	int_detail_size = req_size;
	dsk->p_int_detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA) mp_malloc(req_size);
	dsk->p_int_detail->cbSize = sizeof (SP_INTERFACE_DEVICE_DETAIL_DATA);

	status = SetupDiGetDeviceInterfaceDetail(dsk->int_dev_info, &dsk->int_data, dsk->p_int_detail, int_detail_size, &req_size, NULL);

	if(status == FALSE) {
		hi_dsk_dprintf("hi_win_proc_int: 2nd SetupDiGetDeviceInterfaceDetail failed\n");
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
	int error = hi_dsk_probe_disks();

	if(error != HI_PROBE_OK)
		return error;

	/* Probe volumes */

	return HI_PROBE_OK;
}
