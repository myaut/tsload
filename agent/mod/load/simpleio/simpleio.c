
/*
    This file is part of TSLoad.
    Copyright 2012-2015, Sergey Klyaus, Tune-IT

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



#define LOG_SOURCE "simpleio"
#include <tsload/log.h>

#include <tsload/defs.h>

#include <tsload/mempool.h>
#include <tsload/modapi.h>
#include <tsload/tuneit.h>
#include <tsload/plat/posixdecl.h>

#include <tsload/load/workload.h>

#include <hostinfo/diskinfo.h>
#include <hostinfo/fsinfo.h>
#include <hostinfo/pageinfo.h>

#include <simpleio.h>
#include <iofile.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>


DECLARE_MODAPI_VERSION(MOD_API_VERSION);
DECLARE_MOD_NAME("simpleio");
DECLARE_MOD_TYPE(MOD_TSLOAD);

static const char* rw_wlp_strset[] =  {"read", "write"};

/** Tunable: maximum block size that allowed for requests */
size_t simpleio_max_block_size = 64 * SZ_MB;

/** Tunable: maximum free space that fileio file may occupy (in percent)*/
int fileio_size_threshold = 90;

MODEXPORT wlp_descr_t simpleio_common_params[] = {
	/* Common workload parameters */
	{ WLP_BOOL, WLPF_OPTIONAL,
		WLP_NO_RANGE(),
		WLP_BOOLEAN_DEFAULT(B_FALSE),
		"sync",
		"Use synchronious I/O",
		0
	},
	
	/* Common request parameters */
	{ WLP_STRING_SET, WLPF_REQUEST,
		WLP_STRING_SET_RANGE(rw_wlp_strset),
		WLP_NO_DEFAULT(),
		"rw",
		"Request type",
		offsetof(struct simpleio_request, rw) 
	},
	{ WLP_SIZE, WLPF_REQUEST,
		WLP_INT_RANGE(1, 16 * SZ_MB),
		WLP_NO_DEFAULT(),
		"block_size",
		"Size of block which would be used",
		offsetof(struct simpleio_request, block_size) 
	},
	{ WLP_SIZE, WLPF_REQUEST,
		WLP_NO_RANGE(),
		WLP_NO_DEFAULT(),
		"offset",
		"Block number to be read/written",
		offsetof(struct simpleio_request, offset) 
	}
};

MODEXPORT wlp_descr_t fileio_extra_params[] = {
	{ WLP_FILE_PATH, WLPF_NO_FLAGS,
		WLP_STRING_LENGTH(PATHMAXLEN),
		WLP_NO_DEFAULT(),
		"path",
		"Path to file",
		offsetof(struct fileio_workload, path)
	},
	{ WLP_BOOL, WLPF_OPTIONAL,
		WLP_NO_RANGE(),
		WLP_BOOLEAN_DEFAULT(B_FALSE),
		"sparse",
		"Allow sparse creation of data",
		offsetof(struct fileio_workload, sparse)
	},
	{ WLP_BOOL, WLPF_OPTIONAL,
		WLP_NO_RANGE(),
		WLP_BOOLEAN_DEFAULT(B_FALSE),
		"overwrite",
		"Overwrite file if it is already exists",
		offsetof(struct fileio_workload, overwrite)
	},
	{ WLP_SIZE, WLPF_NO_FLAGS,
		WLP_NO_RANGE(),
		WLP_NO_DEFAULT(),
		"file_size",
		"Size of file which would be benchmarked",
		offsetof(struct fileio_workload, file_size) 
	}
};

MODEXPORT wlp_descr_t diskio_extra_params[] = {
	{ WLP_DISK, WLPF_NO_FLAGS,
		WLP_STRING_LENGTH(PATHMAXLEN),
		WLP_NO_DEFAULT(),
		"disk",
		"Disk object",
		offsetof(struct diskio_workload, disk)
	},
	{ WLP_BOOL, WLPF_OPTIONAL,
		WLP_NO_RANGE(),
		WLP_BOOLEAN_DEFAULT(B_FALSE),
		"rdwr",
		"Allow read-write mode for disk",
		offsetof(struct diskio_workload, rdwr)
	},
	{ WLP_BOOL, WLPF_OPTIONAL,
		WLP_NO_RANGE(),
		WLP_BOOLEAN_DEFAULT(B_FALSE),
		"force",
		"Forcibly run experiment even if disk is used by OS",
		offsetof(struct diskio_workload, force)
	},
};

module_t* self = NULL;

static uint64_t simpleio_get_pagesz(void) {
	hi_page_info_t* pgi = hi_get_pageinfo();
	const int default_mask = (HI_PIF_PAGEINFO | HI_PIF_DEFAULT);
	
	while(pgi->pi_flags != 0) {
		if((pgi->pi_flags & default_mask) == default_mask)
			return pgi->pi_size;
		
		++pgi;
	}
	
	/* Fallback to default x86 value. */
	return 4096;
}

/**
 * Fill file iof with raw data
 */
int simpleio_write_file(workload_t* wl, struct fileio_workload* fiowl, struct simpleio_wldata* simpleio) {
	/* Use page cache as a default block size to tolerate page cache */
	uint64_t pgsz = simpleio_get_pagesz();
	void* block = mp_malloc(pgsz);
	io_file_t* iof = &simpleio->iof;
	
	size_t i = 0, last = fiowl->file_size / pgsz;
	off_t off = 0;
	float one_percent = ((float) last) / 99;
	int last_notify = 0, done = 1;

	for(i = 0; i < last; ++i) {
		if(io_file_pwrite(iof, block, pgsz, off) == -1) {
			wl_notify(wl, WLS_CFG_FAIL, 0, "Failed to write to file '%s'", fiowl->path);
			mp_free(block);
			return -1;
		}
		
		off += pgsz;
		
		done = (int) (i / one_percent);

		if(done > last_notify) {
			wl_notify(wl, WLS_CONFIGURING, done, "Written %" PRIu64 " bytes", (uint64_t) i * pgsz);
			last_notify = done;
		}
	}

	mp_free(block);
	
	return 0;
}

/**
 * Prepare file for simple I/O test
 *
 * Performs various pre-flight checks:
 * 		- Path to file should be absolute
 * 		- Mountpoint on which file resides should exist and should be writeable
 *        (on POSIX all paths will be descendants of root /, so it doesn't apply) 
 *      - File size shouldn't exceed 90% of filesystem free space
 *        (tunable via `fileio_size_threshold`)
 *      - File shouldn't exist if overwrite option is not set or have same size as 
 *        passed in workload parameters
 * */
int simpleio_prepare_file(workload_t* wl, struct fileio_workload* fiowl, struct simpleio_wldata* simpleio) {
	int ret;
	
	io_file_t* iof = &simpleio->iof;
	
	hi_fsinfo_t* fsi;
	uint64_t maxfree;

	simpleio->do_remove = B_FALSE;
	
	if(!path_is_abs(fiowl->path)) {
		wl_notify(wl, WLS_CFG_FAIL, 0, "Path '%s' has to be absolute path", fiowl->path);
		return -1;
	}
	
	fsi = hi_fsinfo_find_bypath(fiowl->path);
	if(fsi == NULL) {
		wl_notify(wl, WLS_CFG_FAIL, 0, "Cannot find filesystem for path '%s'", fiowl->path);
		return -1;
	}
	
	if(fsi->fs_readonly) {
		wl_notify(wl, WLS_CFG_FAIL, 0, "File '%s' resides on read-only filesystem '%s'", 
				  fiowl->path, fsi->fs_mountpoint);
		return -1;
	}
	
	maxfree = fileio_size_threshold * fsi->fs_space_free / 100;
	if(maxfree < fiowl->file_size) {
		wl_notify(wl, WLS_CFG_FAIL, 0, "File size %" PRId64 
				  " is too big -- %d%% of filesystem free space is only %" PRId64, 
			      fiowl->file_size, fileio_size_threshold, maxfree);
		return -1;
	}
	
	ret = io_file_init(iof, IOF_REGULAR, fiowl->path, fiowl->file_size);
	if(ret < 0)
		goto iof_error;
	
	ret = io_file_stat(iof);
	if(ret < 0)
		goto iof_error;
	
	if(iof->iof_exists) {
		if(!fiowl->overwrite) {
			wl_notify(wl, WLS_CFG_FAIL, 0,
					"File '%s' is already exists, SimpleIO won't overwrite it", fiowl->path);
			return -1;
		}
		
		if(iof->iof_file_size != fiowl->file_size) {
			wl_notify(wl, WLS_CFG_FAIL, 0,
					"File '%s' has different size: %"PRIu64"b is expected, %"PRIu64"b is actual", 
					fiowl->path, fiowl->file_size, iof->iof_file_size);
			return -1;
		}
		
		/* TODO: Some additional checks to prevent ovewriting important files 
		 * i.e. check file owner and user running TSLoad */
	}

	logmsg(LOG_INFO, "Creating file '%s' with size %" PRIu64, fiowl->path, (uint64_t) fiowl->file_size);

	ret = io_file_open(iof, B_TRUE, fiowl->sync);
	if(ret < 0)
		goto iof_error;
	
	wl_notify(wl, WLS_CONFIGURING, 1, 
			  "Opened file '%s' on filesystem '%s' type: %s on device %s", 
		      fiowl->path, fsi->fs_mountpoint, fsi->fs_type, 
		      (fsi->fs_device) ? fsi->fs_device->d_disk_name : fsi->fs_devpath);
	
	/* File existed at the moment we opened it, no need to overwrite it */
	if(iof->iof_exists) 
		return 0;
	
	simpleio->do_remove = B_TRUE;
	
	if(fiowl->sparse) {
		char eof = 3;
		
		ret = io_file_seek(iof, fiowl->file_size);
		if(ret < 0)
			goto iof_error;
		
		if(io_file_pwrite(iof, &eof, 1, fiowl->file_size) == -1) {
			wl_notify(wl, WLS_CFG_FAIL, 0, "Failed to write to file '%s'", fiowl->path);
			goto error;
		}
		
		return 0;
	}
	
	ret = simpleio_write_file(wl, fiowl, simpleio);
	if(ret < 0)
		io_file_close(&simpleio->iof, B_TRUE);
	
	return ret;

iof_error:
	wl_notify(wl, WLS_CFG_FAIL, 0, 
			  (iof->iof_error_msg) ? iof->iof_error_msg : "Unknown iofile error");
error:
	io_file_close(&simpleio->iof, B_FALSE);
	return -1;
}

MODEXPORT int fileio_wl_config(workload_t* wl) {
	int ret;
	
	struct fileio_workload* fiowl = (struct fileio_workload*) wl->wl_params;
	struct simpleio_wldata* simpleio = mp_malloc(sizeof(struct simpleio_wldata));
	
	ret = simpleio_prepare_file(wl, fiowl, simpleio);
	if(ret != 0) {
		mp_free(simpleio);
		return -1;
	}
	
	wl_notify(wl, WLS_CONFIGURING, 99, "Prepared file '%s'", fiowl->path);
	
	wl->wl_private = simpleio;
	
	return 0;
}

static void diskio_wl_notify(workload_t* wl, hi_dsk_info_t* di) {
	AUTOSTRING char* extra_info;
	aas_init(&extra_info);
	
	if(di->d_model) {
		aas_printf(&extra_info, "(model: %s)", di->d_model);
	}
	else if(di->d_bus_type) {
		aas_printf(&extra_info, "connected via %s", di->d_bus_type);
	}
	else if(di->d_port) {
		aas_printf(&extra_info, "connected to %s", di->d_port);
	}
	else {
		aas_set(&extra_info, "");
	}
	
	wl_notify(wl, WLS_CONFIGURING, 99, "Opened disk device '%s' %s", di->d_path, extra_info);
	
	aas_free(&extra_info);
}

/**
 * Performs various pre-flight checks:
 * 		- Disk object shouldn't be a pool 
 * 		- Disk or its partitions shouldn't contain filesystem, or a pool/volume
 * 		- Access flags should be OK (not yet implemented)
 */
MODEXPORT int diskio_wl_config(workload_t* wl) {
	struct diskio_workload* diowl = (struct diskio_workload*) wl->wl_params;
	struct simpleio_wldata* simpleio = mp_malloc(sizeof(struct simpleio_wldata));
	
	hi_dsk_info_t* di = (hi_dsk_info_t*) diowl->disk;
	hi_object_t* overlap;
	
	io_file_t* iof = &simpleio->iof;
	
	int ret;
	
	if(di->d_type == HI_DSKT_POOL) {
		wl_notify(wl, WLS_CFG_FAIL, 0, "Cannot use disk pool '%s' in diskio!", di->d_disk_name);
		goto error;
	}
	
	/* TODO: `sda` and similiar names are non-portable
	 * implement criteria based wlparam binding for disks i.e. by uuid, disk model, bus, etc.*/
	
	overlap = hi_dsk_check_overlap(di);
	if(overlap != NULL) {
		wl_notify(wl, (diowl->force)? WLS_CONFIGURING : WLS_CFG_FAIL, 0, 
				  "Disk '%s' overlaps with object '%s'!", di->d_disk_name, overlap->name);
		
		if(!diowl->force) 
			goto error;
	}
	
	ret = io_file_init(iof, IOF_BLOCKDEV, di->d_path, di->d_size);
	if(ret < 0)
		goto iof_error;
	
	/* TODO: Check access flags */
	
	ret = io_file_open(iof, diowl->rdwr, diowl->sync);
	if(ret < 0)
		goto iof_error;
	
	simpleio->do_remove = B_FALSE;
	
	diskio_wl_notify(wl, di);
	
	wl->wl_private = simpleio;
	
	return 0;

iof_error:
	wl_notify(wl, WLS_CFG_FAIL, 0, 
			  (iof->iof_error_msg) ? iof->iof_error_msg : "Unknown iofile error");
	io_file_close(&simpleio->iof, B_FALSE);
error:
	mp_free(simpleio);
	return -1;
}

MODEXPORT int simpleio_wl_unconfig(workload_t* wl) {
	struct simpleio_wldata* simpleio = (struct simpleio_wldata*) wl->wl_private;
	
	if(simpleio) {
		io_file_close(&simpleio->iof, simpleio->do_remove);
		mp_free(simpleio);
	}
	
	wl->wl_private = NULL;
	
	return 0;
}

static void* simpleio_create_block(size_t blksz, uint32_t seed) {
	void* block = mp_malloc(blksz);
	
	/* Fill block with some data to confuse deduplication/compression */
	uint32_t* blkdata = (uint32_t*) block;
	size_t i;
	
	for(i = 0; i < blksz / sizeof(uint32_t); ++i) {
		*blkdata = ((uint32_t)(uintptr_t) blkdata) ^ seed;
		seed = ~seed + (seed & 0x7fff);
		++blkdata;
	}
	
	return block;
}

MODEXPORT int simpleio_run_request(request_t* rq) {
	struct simpleio_wldata* simpleio = (struct simpleio_wldata*) rq->rq_workload->wl_private;
	struct simpleio_request* siorq = (struct simpleio_request*) rq->rq_params;
	
	size_t blksz = min(siorq->block_size, simpleio_max_block_size);
	
	int ret;	
	
	void* block = simpleio_create_block(blksz, siorq->offset);
	
	/* TODO: make filling block and aligning offset as parameters */
	
	siorq->block_size = blksz;
	siorq->offset = (siorq->offset * blksz) % (simpleio->iof.iof_file_size - blksz);
	
	if(siorq->rw == 0) {
		ret = io_file_pread(&simpleio->iof, block, blksz, siorq->offset);
	}
	else {
		ret = io_file_pwrite(&simpleio->iof, block, blksz, siorq->offset);
	}

	mp_free(block);
	
	return (ret == blksz)? 0 : -1;
}


wl_type_t fileio_wlt = {
	"fileio",							/* wlt_name */

	WLC_FILESYSTEM_RW,					/* wlt_class */
	"Simple filesystem read/write workload",

	NULL,								/* wlt_params */
	sizeof(struct fileio_workload),		/* wlt_params_size*/
	sizeof(struct simpleio_request),	/* wlt_params_rqsize*/

	fileio_wl_config,					/* wlt_wl_config */
	simpleio_wl_unconfig,				/* wlt_wl_unconfig */

	NULL,								/* wlt_wl_step */

	simpleio_run_request				/* wlt_run_request */
};

wl_type_t diskio_wlt = {
	"diskio",							/* wlt_name */

	WLC_DISK_RW,						/* wlt_class */
	"Simple block device workload",

	NULL,								/* wlt_params */
	sizeof(struct diskio_workload),		/* wlt_params_size*/
	sizeof(struct simpleio_request),	/* wlt_params_rqsize*/

	diskio_wl_config,					/* wlt_wl_config */
	simpleio_wl_unconfig,				/* wlt_wl_unconfig */

	NULL,								/* wlt_wl_step */
	
	simpleio_run_request				/* wlt_run_request */
};

static
wlp_descr_t* simpleio_create_params_impl(wlp_descr_t* extra_params, int extra_count) {
	wlp_descr_t* params;
	
	int common_count = sizeof(simpleio_common_params) / sizeof(wlp_descr_t);
	int count = common_count + extra_count + 1;

	params = mp_malloc(count * sizeof(wlp_descr_t));
	
	memcpy(params, simpleio_common_params, common_count * sizeof(wlp_descr_t));
	memcpy(params + common_count, extra_params, extra_count * sizeof(wlp_descr_t));
	params[count - 1].type = WLP_NULL;
	
	return params;
}

#define simpleio_create_params(extra_params)					\
		simpleio_create_params_impl(extra_params,				\
				sizeof(extra_params) / sizeof(wlp_descr_t))

MODEXPORT int mod_config(module_t* mod) {
	logmsg(LOG_INFO, "simpleio module is loaded");

	self = mod;
	
	tuneit_set_int(size_t, simpleio_max_block_size);
	tuneit_set_int(int, fileio_size_threshold);
	
	if(fileio_size_threshold > 100)
		fileio_size_threshold = 100;
	
	fileio_wlt.wlt_params = simpleio_create_params(fileio_extra_params);
	fileio_wlt.wlt_params[0].off = offsetof(struct fileio_workload, sync);
	wl_type_register(mod, &fileio_wlt);
	
	diskio_wlt.wlt_params = simpleio_create_params(diskio_extra_params);
	diskio_wlt.wlt_params[0].off = offsetof(struct diskio_workload, sync);
	wl_type_register(mod, &diskio_wlt);

	return 0;
}

MODEXPORT int mod_unconfig(module_t* mod) {
	wl_type_unregister(mod, &fileio_wlt);
	mp_free(fileio_wlt.wlt_params);
	
	wl_type_unregister(mod, &diskio_wlt);
	mp_free(diskio_wlt.wlt_params);

	return 0;
}

