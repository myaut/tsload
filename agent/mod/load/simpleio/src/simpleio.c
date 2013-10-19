/*
 * simpleio.c
 *
 *  Created on: 06.11.2012
 *      Author: myaut
 */

#define LOG_SOURCE "simpleio"
#include <log.h>

#include <mempool.h>
#include <defs.h>
#include <workload.h>
#include <modules.h>
#include <modapi.h>

#include <simpleio.h>

#include <plat/posixdecl.h>

#include <stdlib.h>
#include <string.h>

#include <errno.h>

DECLARE_MODAPI_VERSION(MOD_API_VERSION);
DECLARE_MOD_NAME("simpleio");
DECLARE_MOD_TYPE(MOD_TSLOAD);

static char* tests[] =  {"read", "write"};

MODEXPORT wlp_descr_t simpleio_params[] = {
	{ WLP_SIZE, WLPF_OPTIONAL,
		WLP_INT_RANGE(1, 1 * SZ_TB),
		WLP_INT_DEFAULT(0),
		"filesize",
		"Size of file which would be benchmarked",
		offsetof(struct simpleio_workload, file_size) },
	{ WLP_SIZE, WLPF_NO_FLAGS,
		WLP_INT_RANGE(1, 16 * SZ_MB),
		WLP_NO_DEFAULT(),
		"blocksize",
		"Size of block which would be used",
		offsetof(struct simpleio_workload, block_size)},
	{ WLP_FILE_PATH, WLPF_NO_FLAGS,
		WLP_STRING_LENGTH(512),
		WLP_NO_DEFAULT(),
		"path",
		"Path to file",
		offsetof(struct simpleio_workload, path)
	},
	{ WLP_BOOL, WLPF_NO_FLAGS,
		WLP_NO_RANGE(),
		WLP_NO_DEFAULT(),
		"sparse",
		"Allow sparse creation of data",
		offsetof(struct simpleio_workload, sparse)
	},
	{ WLP_BOOL, WLPF_NO_FLAGS,
		WLP_NO_RANGE(),
		WLP_NO_DEFAULT(),
		"sync",
		"Use synchronious I/O",
		offsetof(struct simpleio_workload, sync)
	},
	{ WLP_NULL }
};

module_t* self = NULL;

/**
 * Fill file iof with raw data
 */
int simpleio_write_file(workload_t* wl, io_file_t* iof, struct simpleio_workload* simpleio) {
	size_t i = 0, last = simpleio->file_size / simpleio->block_size;
	float one_percent = ((float) last) / 100;
	int last_notify = 0, done = 0;

	for(i = 0; i < last; ++i) {
		if(io_file_write(iof, simpleio->block, simpleio->block_size) == -1) {
			wl_notify(wl, WLS_FAIL, 0,
					"Failed to write: %s", strerror(errno));
			return -1;
		}

		done = (int) (i / one_percent);

		if(done > last_notify) {
			wl_notify(wl, WLS_CONFIGURING, done,
					"Written %lu bytes", i * simpleio->block_size);
			last_notify = done;
		}
	}

	return 0;
}

/**
 * Prepare file for simple I/O test
 *
 * Checks if file already exists. If no writes or do sparse creation of new file.
 * */
int simpleio_prepare_file(workload_t* wl, struct simpleio_workload* simpleio) {
	int ret = io_file_stat(&simpleio->iof, simpleio->path);

	if(ret < 0) {
		wl_notify(wl, WLS_FAIL, 0,
					"Couldn't stat file %s: %s", simpleio->path, strerror(errno));
		return -1;
	}

	simpleio->do_remove = B_FALSE;

	if(simpleio->iof.iof_exists) {
		if(simpleio->file_size != 0 &&
		   simpleio->iof.iof_file_type != IOF_REGULAR) {
			wl_notify(wl, WLS_FAIL, 0,
						"File %s is not regular, while it's creation is requested", simpleio->path);
			return -1;
		}

		/* File already exists and has correct size, no need for it's creation*/
		if(simpleio->file_size == simpleio->iof.iof_file_size)
			return 0;
	}

	logmsg(LOG_INFO, "Creating file %s with size %llu", simpleio->path, simpleio->file_size);

	if(io_file_open(&simpleio->iof, B_TRUE, B_FALSE) < 0) {
		wl_notify(wl, WLS_FAIL, 0,
					"Couldn't open file %s for writing: %s", simpleio->path, strerror(errno));
		return -1;
	}

	if(simpleio->sparse) {
		io_file_seek(&simpleio->iof, simpleio->file_size);
	}
	else {
		simpleio_write_file(wl, &simpleio->iof, simpleio);
	}

	simpleio->do_remove = B_TRUE;

	return io_file_close(&simpleio->iof, B_FALSE);
}

static int simpleio_wl_config(workload_t* wl, struct simpleio_workload* simpleio) {
	int ret = 0;

	simpleio->block = mp_malloc(simpleio->block_size);

	if(simpleio_prepare_file(wl, simpleio) == -1)
		return -1;

	ret = io_file_stat(&simpleio->iof, simpleio->path);

	if(ret == -1 || !simpleio->iof.iof_exists) {
		wl_notify(wl, WLS_FAIL, 0,
					"Failed to create target file %s: %s", simpleio->path, strerror(errno));
		return -1;
	}

	simpleio->file_size = simpleio->iof.iof_file_size;

	return 0;
}

MODEXPORT int simpleio_wl_config_read(workload_t* wl) {
	struct simpleio_workload* simpleio = (struct simpleio_workload*) wl->wl_params;

	simpleio_wl_config(wl, simpleio);

	return io_file_open(&simpleio->iof, B_FALSE, simpleio->sync);
}

MODEXPORT int simpleio_wl_config_write(workload_t* wl) {
	struct simpleio_workload* simpleio = (struct simpleio_workload*) wl->wl_params;

	simpleio_wl_config(wl, simpleio);

	return io_file_open(&simpleio->iof, B_TRUE, simpleio->sync);
}

MODEXPORT int simpleio_wl_unconfig(workload_t* wl) {
	struct simpleio_workload* simpleio = (struct simpleio_workload*) wl->wl_params;

	io_file_close(&simpleio->iof, simpleio->do_remove);

	mp_free(simpleio->block);

	return 0;
}

MODEXPORT int simpleio_seek_block(struct simpleio_workload* simpleio) {
	/* Select random block */
	unsigned block = rand() % (simpleio->file_size / simpleio->block_size);

	return io_file_seek(&simpleio->iof, block * simpleio->block_size);
}

MODEXPORT int simpleio_run_request_read(request_t* rq) {
	struct simpleio_workload* simpleio = (struct simpleio_workload*) rq->rq_workload->wl_params;
	int ret;

	simpleio_seek_block(simpleio);

	ret = io_file_read(&simpleio->iof, simpleio->block, simpleio->block_size);

	return (ret == simpleio->block_size)? 0 : -1;
}

MODEXPORT int simpleio_run_request_write(request_t* rq) {
	struct simpleio_workload* simpleio = (struct simpleio_workload*) rq->rq_workload->wl_params;
	int ret;

	simpleio_seek_block(simpleio);

	ret = io_file_write(&simpleio->iof, simpleio->block, simpleio->block_size);

	return (ret == simpleio->block_size)? 0 : -1;
}

wl_type_t simpleio_wlt_read = {
	"simpleio_read",					/* wlt_name */

	WLC_FILESYSTEM_RW | WLC_DISK_RW,	/* wlt_class */

	simpleio_params,					/* wlt_params */
	sizeof(struct simpleio_workload),	/* wlt_params_size*/

	simpleio_wl_config_read,			/* wlt_wl_config */
	simpleio_wl_unconfig,				/* wlt_wl_unconfig */

	NULL,								/* wlt_wl_step */

	simpleio_run_request_read			/* wlt_run_request */
};

wl_type_t simpleio_wlt_write = {
	"simpleio_write",					/* wlt_name */

	WLC_FILESYSTEM_RW | WLC_DISK_RW,	/* wlt_class */

	simpleio_params,					/* wlt_params */
	sizeof(struct simpleio_workload),	/* wlt_params_size*/

	simpleio_wl_config_write,			/* wlt_wl_config */
	simpleio_wl_unconfig,				/* wlt_wl_unconfig */

	NULL,								/* wlt_wl_step */

	simpleio_run_request_write			/* wlt_run_request */
};

MODEXPORT int mod_config(module_t* mod) {
	logmsg(LOG_INFO, "simpleio module is loaded");

	self = mod;

	wl_type_register(mod, &simpleio_wlt_read);
	wl_type_register(mod, &simpleio_wlt_write);

	return 0;
}

MODEXPORT int mod_unconfig(module_t* mod) {
	wl_type_unregister(mod, &simpleio_wlt_read);
	wl_type_unregister(mod, &simpleio_wlt_write);

	return 0;
}
