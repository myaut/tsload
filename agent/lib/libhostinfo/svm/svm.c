
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

#include <tsload/defs.h>

#include <tsload/pathutil.h>
#include <tsload/autostring.h>
#include <tsload/posixdecl.h>
#include <tsload/mempool.h>

#include <hostinfo/fsinfo.h>
#include <hostinfo/diskinfo.h>

#include <hitrace.h>

#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include <sys/dkio.h>
#include <sys/vtoc.h>


/* Unfortunately, libmeta supports only 32-bit builds, and also
 * it has very complex API, so instead of imitating it, we will
 * use direct approach: call `metastat -p`.
 * 
 * It prints metainit arguments that are easy to parse. */

#ifndef METASTAT_PATH
#define METASTAT_PATH "/usr/sbin/metastat"
#endif

#define HISVMCOMPCAPACITY		4
#define HISVMBUFLEN		64

/* SVM components can be c0t0d0s0 disk, dXXX device or path to it */
#define IS_SVM_COMPONENT(devname)						\
	((devname)[0] == 'c' || (devname)[0] == 'd' || (devname)[0] == '/')

typedef enum {
	SVM_UNKNOWN,
	SVM_MIRROR,
	SVM_STRIPE,
	SVM_SOFT_PARTITION,
	
	/* TODO: support HSP, trans, metadb, etc. */
} hi_svm_devtype_t;

typedef struct {
	char* devname;
	
	char** components;
	int count;
	int capacity;
	
	hi_svm_devtype_t devtype;
	
	list_node_t node;
} hi_svm_device_t;

typedef struct {
	list_head_t devices;
	const char* setname;
} hi_svm_set_t;

typedef struct {
	char* str;
	int slen;
	int eol;
} hi_svm_buffer_t;

hi_svm_device_t* hi_svm_create_dev(char* devname) {
	hi_svm_device_t* dev = (hi_svm_device_t*) mp_malloc(sizeof(hi_svm_device_t));
	
	dev->devname = devname;
	
	dev->count = 0;
	dev->capacity = HISVMCOMPCAPACITY;
	dev->components = mp_malloc(HISVMCOMPCAPACITY * sizeof(char*));
	
	dev->devtype = SVM_UNKNOWN;
	
	list_node_init(&dev->node);
	
	return dev;
}

void hi_svm_add_component(hi_svm_device_t* dev, char* component) {
	if(dev->count == dev->capacity) {
		dev->capacity += HISVMCOMPCAPACITY;
		dev->components = mp_realloc(dev->components, dev->capacity * sizeof(char*));
	}
	
	dev->components[dev->count] = component;
	dev->count++;
	
	hi_dsk_dprintf("hi_svm_add_component: added component '%s' -> '%s'\n", 
				   component, dev->devname);
}

void hi_svm_destroy_dev(hi_svm_device_t* dev) {
	hi_dsk_dprintf("hi_svm_destroy_dev: Destroyed device '%s'\n", dev->devname);
	
	list_del(&dev->node);
	
	mp_free(dev->components);
	mp_free(dev);
}

/* SVM buffers - a tool to process metastat output.
 * Instead of keeping them on stack, parser state is saved in buffer:
 * 		- str points to last read component
 * 		- init() initializes buffer
 * 		- get() re-initializes buffer and return str 
 *              (to be used in hierarchy of hi_svm_device_t)
 * 		- read() reads until next space character
 * 		- free() releases resources
 * 
 * eol is is special flag that cancels subsequent svm_buf_find_eol()
 */

STATIC_INLINE void svm_buf_init(hi_svm_buffer_t* buf) {
	buf->str = mp_malloc(HISVMBUFLEN);
	buf->slen = HISVMBUFLEN;
	buf->eol = '\0';
}

STATIC_INLINE char* svm_buf_get(hi_svm_buffer_t* buf) {
	char* str = buf->str;
	
	buf->str = mp_malloc(HISVMBUFLEN);
	buf->slen = HISVMBUFLEN;
	
	return str;
}

STATIC_INLINE char* svm_buf_read(hi_svm_buffer_t* buf, FILE* f) {
	int chr;
	int i = 0;
	
	while(!feof(f)) {
		/* Read field until first space character found
		 * if we met whitespace before field started (i == 0), ignore it 
		 * If we met end-of-*, stop reading */
		
		chr = fgetc(f);
		if(chr == EOF || chr == '\n') {
			buf->eol = chr;
			if(chr == EOF)
				return NULL;
		}
		if(isspace(chr) && i == 0)
			continue;
		if(isspace(chr))
			break;
		
		buf->str[i++] = chr;
		
		if((i + 1) > buf->slen) {
			buf->slen += HISVMBUFLEN;
			buf->str = mp_realloc(buf->str, buf->slen);
		}
	}
	
	buf->str[i] = '\0';	
	hi_dsk_dprintf("svm_buf_read: '%s' eol=%d\n", buf->str, buf->eol);
	
	return buf->str;
}

STATIC_INLINE void svm_buf_fini(hi_svm_buffer_t* buf) {
	mp_free(buf->str);
}

int svm_buf_find_eol(hi_svm_buffer_t* buf, FILE* f) {
	int chr = buf->eol;
	
	hi_dsk_dprintf("svm_buf_find_eol: eol=%d\n", chr);
	
	if(chr != '\0') {
		buf->eol = '\0';
		return chr;
	}
	
	while(!feof(f)) {
		chr = fgetc(f);
		if(chr == '\n' || chr == EOF)
			return chr;
	}
	
	return '\0';
}

int hi_svm_proc_mirror(hi_svm_set_t* set, FILE* metaf, hi_svm_device_t* mirror, hi_svm_buffer_t* buf) {
	char* subdev = NULL;
	
	mirror->devtype = SVM_MIRROR;
	
	do {
		subdev = svm_buf_read(buf, metaf);
		
		if(subdev != NULL) {
			if(!IS_SVM_COMPONENT(subdev))
				break;
			
			hi_dsk_dprintf("hi_svm_proc_mirror: Found submirror '%s' for mirror '%s'\n", subdev, mirror->devname);
			hi_svm_add_component(mirror, svm_buf_get(buf));
		}
	} while(subdev != NULL);
	
	svm_buf_find_eol(buf, metaf);
	
	return HI_PROBE_OK;
}

int hi_svm_proc_stripe(hi_svm_set_t* set, FILE* metaf, hi_svm_device_t* dev, hi_svm_buffer_t* buf, long nrows) {
	long ndevs;
	int rowid, devid;
	
	char* ndevstr = NULL;
	char* stripe = NULL;
	
	int chr;

	dev->devtype = SVM_STRIPE;
	
	hi_dsk_dprintf("hi_svm_proc_stripe: Found metadevice '%s' with %ld rows\n", dev->devname, nrows);
	
	for(rowid = 0; rowid < nrows; ++rowid) {
		ndevstr = svm_buf_read(buf, metaf);
		if(ndevstr == NULL)
			return HI_PROBE_ERROR;
		
		ndevs = strtol(ndevstr, NULL, 10);
		
		hi_dsk_dprintf("hi_svm_proc_stripe: Metadevice '%s' has %ld subdevs at row %d\n", 
					   dev->devname, ndevs, rowid);
		
		for(devid = 0; devid < ndevs; ++devid) {
			stripe = svm_buf_read(buf, metaf);
			if(stripe == NULL) 
				return HI_PROBE_ERROR;
			if(!IS_SVM_COMPONENT(stripe))
				return HI_PROBE_ERROR;
			
			hi_dsk_dprintf("hi_svm_proc_stripe: Found stripe '%s' for metadevice '%s'\n", stripe, dev->devname);
			hi_svm_add_component(dev, svm_buf_get(buf));
		}
		
		if(svm_buf_find_eol(buf, metaf) == EOF)
			break;
	}
	
	return HI_PROBE_OK;
}

int hi_svm_proc_softpart(hi_svm_set_t* set, FILE* metaf, hi_svm_device_t* softpart, hi_svm_buffer_t* buf) {
	char* subdev = NULL;
	
	softpart->devtype = SVM_SOFT_PARTITION;
	
	subdev = svm_buf_read(buf, metaf);
	
	if(subdev != NULL && IS_SVM_COMPONENT(subdev)) {		
		hi_dsk_dprintf("hi_svm_proc_mirror: Found component '%s' for soft partition '%s'\n", subdev, softpart->devname);
		hi_svm_add_component(softpart, svm_buf_get(buf));
	}

	svm_buf_find_eol(buf, metaf);
	
	return HI_PROBE_OK;
}

int hi_svm_proc_metadev(hi_svm_set_t* set, FILE* metaf) {
	hi_svm_buffer_t buf;
	
	int err = HI_PROBE_OK;
	
	char* devname = NULL;
	char* devopt = NULL;
	
	hi_svm_device_t* dev = NULL;
	
	svm_buf_init(&buf);
	
	/* Read device name */
	if(svm_buf_read(&buf, metaf) == NULL)
		goto error;
	
	devname = svm_buf_get(&buf);
	hi_dsk_dprintf("hi_svm_proc_metadev: Found device '%s'\n", devname);
	
	/* Read first option -- see meta_get_init_type() in meta_init.c */
	devopt = svm_buf_read(&buf, metaf);
	if(devopt == NULL) {
		hi_dsk_dprintf("hi_svm_proc_metadev: Error reading devopt for device '%s'\n", devname);
		goto error;
	}
	
	dev = hi_svm_create_dev(devname);
	
	if(strcmp(devopt, "-m") == 0) {
		err = hi_svm_proc_mirror(set, metaf, dev, &buf);
	}
	else if(isdigit(devopt[0])) {
		long nrows = strtol(devopt, NULL, 10);
		err = hi_svm_proc_stripe(set, metaf, dev, &buf, nrows);
	}
	else if(strcmp(devopt, "-p") == 0) {
		err = hi_svm_proc_softpart(set, metaf, dev, &buf);
	}
	else {
		hi_dsk_dprintf("hi_svm_proc_metadev: Unsupported device option '%s'\n", devopt);
		goto error;
	}
	
	hi_dsk_dprintf("hi_svm_proc_metadev: Processed device '%s'\n", dev->devname);
	list_add_tail(&dev->node, &set->devices);
	
end:
	svm_buf_fini(&buf);
	return err;

error:
	if(dev)
		hi_svm_destroy_dev(dev);
	else
		mp_free(devname);
	svm_buf_fini(&buf);
	return HI_PROBE_ERROR;
}

uint64_t hi_svm_get_size(hi_svm_set_t* set, hi_svm_device_t* dev) {
	AUTOSTRING char* path;
	int fd;
	
	struct vtoc	dkvtoc;
	unsigned short sectorsz;
	struct dk_geom	dkgeom;
	
	int ret;
	uint64_t sz = 0;
	
	/* TODO: support for EFI labels */
	
	if(set->setname)
		aas_printf(aas_init(&path), "/dev/md/%s/rdsk/%s", set->setname, dev->devname);
	else
		aas_printf(aas_init(&path), "/dev/md/rdsk/%s", dev->devname);
	
	fd = open(path, O_RDONLY | O_NDELAY);	
	if(fd < 0) {
		hi_dsk_dprintf("hi_svm_get_size: open(%s) failed: error %d\n", path, errno);
		aas_free(&path);
		return 0;
	}
	
	ret = ioctl(fd, DKIOCGVTOC, &dkvtoc);
	if(ret >= 0)
		sectorsz = dkvtoc.v_sectorsz;
	else
		sectorsz = 512;
	
	ret = ioctl(fd, DKIOCGGEOM, &dkgeom);
	
	if(ret >= 0) {
		sz = (uint64_t) dkgeom.dkg_ncyl * dkgeom.dkg_nhead;
		sz *= dkgeom.dkg_nsect;
		sz *= sectorsz;
		
		return sz;
	}
	else {
		hi_dsk_dprintf("hi_svm_get_size: ioctl(%s, DKIOCGGEOM) failed: error %d\n", path, errno);
	}
	
	aas_free(&path);
	close(fd);
	return sz;
}

hi_dsk_info_t* hi_svm_bind_dev(hi_svm_set_t* set, hi_svm_device_t* dev) {
	hi_dsk_info_t* di;
	
	path_split_iter_t iter;
	
	int subid;
	const char* subname;
	hi_dsk_info_t* subdi = NULL;
	hi_svm_device_t* subdev;
	
	/* Create DiskInfo object */
	di = hi_dsk_create();
	
	hi_dsk_dprintf("hi_svm_bind_dev: Created %s of type %d\n", dev->devname, dev->devtype);
	
	aas_printf(&di->d_disk_name, "md:%s", dev->devname);
	
	switch(dev->devtype) {
	case SVM_MIRROR:
		aas_set(&di->d_bus_type, "SVM:MIRROR");
		break;
	case SVM_STRIPE:
		aas_set(&di->d_bus_type, "SVM:STRIPE");
		break;
	case SVM_SOFT_PARTITION:
		aas_set(&di->d_bus_type, "SVM:SP");
		break;
	}
	
	if(set->setname)
		aas_printf(&di->d_path, "/dev/md/%s/dsk/%s", set->setname, dev->devname);
	else
		aas_printf(&di->d_path, "/dev/md/dsk/%s", dev->devname);
	
	di->d_type = HI_DSKT_VOLUME;
	di->d_size = hi_svm_get_size(set, dev);
	
	hi_dsk_add(di);
	
	/* Walk over subcomponents -- try to find subcomponent in
	 * SVM's list and recursively create subdevice -- if fail to do
	 * so -- walk over hi_dsk list */
	for(subid = 0; subid < dev->count; ++subid) {
		subdi = NULL;
		subname = dev->components[subid];
		
		if(subname[0] == '/') {
			subname = path_basename(&iter, subname);
		}
		
		list_for_each_entry(hi_svm_device_t, subdev, &set->devices, node) {
			if(strcmp(subdev->devname, subname) == 0) {
				subdi = hi_svm_bind_dev(set, subdev);
				break;
			}
		}
		
		if(subdi == NULL) {
			subdi = hi_dsk_find(subname);
		}
		
		if(subdi == NULL) {
			hi_dsk_dprintf("hi_svm_bind_dev: Failed to bind %s -> %s -- devices was not found\n", 
						   dev->devname, subname);
			continue;
		}
		
		hi_dsk_attach(subdi, di);
	}
	
	hi_svm_destroy_dev(dev);
	
	return di;
}

void hi_svm_bind_devs(hi_svm_set_t* set) {
	hi_svm_device_t* dev;
	hi_dsk_info_t* di;
	
	while(!list_empty(&set->devices)) {
		dev = list_first_entry(hi_svm_device_t, &set->devices, node);
		hi_svm_bind_dev(set, dev);
	}
}

int hi_svm_proc_metaset(const char* setname) {
	AUTOSTRING char* cmd;
	FILE* metaf;
	
	hi_svm_set_t set;
	
	/* Read & parse metastat -p */
	aas_init(&cmd);
	if(setname)
		aas_printf(&cmd, "%s -s %s -p", METASTAT_PATH, setname);
	else
		aas_printf(&cmd, "%s -p", METASTAT_PATH);
	
	hi_dsk_dprintf("hi_svm_probe_metaset: probing metastat with command '%s'\n", cmd);
	metaf = popen(cmd, "r");
	aas_free(&cmd);
	
	if(metaf == NULL) {
		hi_dsk_dprintf("hi_svm_probe_metaset: failed to popen(): error %d\n", errno);
		return HI_PROBE_ERROR;
	}
	
	set.setname = setname;
	list_head_init(&(set.devices), "svm-devs");
	
	while(!feof(metaf)) {
		int chr;
		
		hi_svm_proc_metadev(&set, metaf);
	}
	
	pclose(metaf);
	
	/* Process & bind devices */
	hi_svm_bind_devs(&set);
	
	return HI_PROBE_OK;
}

int hi_svm_probe(void) {
	hi_svm_proc_metaset(NULL);
	
	/* TODO: support metasets */
	
	return HI_PROBE_OK;
}