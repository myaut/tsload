
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

#define LOG_SOURCE "blksz-exp"
#include <tsload/log.h>

#include <tsload/defs.h>
#include <tsload/modapi.h>

#include <tsload/mempool.h>

#include <tsload/load/randgen.h>
#include <tsload.h>

DECLARE_MODAPI_VERSION(MOD_API_VERSION);
DECLARE_MOD_NAME("blksz-exp");
DECLARE_MOD_TYPE(MOD_TSLOAD);

struct module* self = NULL;

typedef struct rv_blksz_exp {
	randvar_t* rv;
	long min_blksz;
	long max_blksz;
} rv_blksz_exp_t;

int rv_init_blksz_exp(randvar_t* rv) {
	rv_blksz_exp_t* rvb = (rv_blksz_exp_t*) mp_malloc(sizeof(rv_blksz_exp_t));

	if(rvb == NULL)
		return 1;

	rvb->min_blksz = 512;
	rvb->max_blksz = 16 * SZ_MB;
	
	rvb->rv = rv_create(randvar_find("exponential"), rv->rv_generator);
	
	if(rvb->rv == NULL) {
		mp_free(rvb);
		return 1;
	}
	
	rv->rv_private = rvb;

	return 0;
}

void rv_destroy_blksz_exp(randvar_t* rv) {
	rv_blksz_exp_t* rvb = (rv_blksz_exp_t*) rv->rv_private;
	
	rv_destroy(rvb->rv);
	
	mp_free(rv->rv_private);
}

int rv_set_int_blksz_exp(randvar_t* rv, const char* name, long value) {
	rv_blksz_exp_t* rvb = (rv_blksz_exp_t*) rv->rv_private;
	
	if(strcmp(name, "mean") == 0) {
		if(value < 1)
			return RV_INVALID_PARAM_VALUE;
		
		return rv_set_double(rvb->rv, "rate", 1. / ((double) value));
	}
	if(strcmp(name, "min-blksz") == 0) {
		if(value < 1)
			return RV_INVALID_PARAM_VALUE;

		rvb->min_blksz = value;
	}
	else if(strcmp(name, "max-blksz") == 0) {
		if (value < 1)
			return RV_INVALID_PARAM_VALUE;

		rvb->max_blksz = value;
	}
	else {
		return RV_INVALID_PARAM_NAME;
	}

	return RV_PARAM_OK;
}

double rv_variate_double_blksz_exp(randvar_t* rv, double u) {
	rv_blksz_exp_t* rvb = (rv_blksz_exp_t*) rv->rv_private;
	
	long v = (long) rv_variate_double(rvb->rv);
	
	if(v < rvb->min_blksz)
		return rvb->min_blksz;
	if(v > rvb->max_blksz)
		return rvb->max_blksz;
	
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	
	return (double) v;
}

tsload_param_t rv_blksz_exp_params[] = {
	{ TSLOAD_PARAM_INTEGER, "mean", "cannot be negative" },
	{ TSLOAD_PARAM_INTEGER, "min-blksz", "cannot be negative, and should be power of 2" },
	{ TSLOAD_PARAM_INTEGER, "max-blksz", "cannot be negative, and should be power of 2" },
	{ TSLOAD_PARAM_NULL, NULL, NULL }
};

randvar_class_t rv_blksz_exp_class = {
	RV_CLASS_HEAD("blksz-exp", rv_blksz_exp_params),
	
	SM_INIT(.rv_init, rv_init_blksz_exp),
	SM_INIT(.rv_destroy, rv_destroy_blksz_exp),

	SM_INIT(.rv_set_int, rv_set_int_blksz_exp),
	SM_INIT(.rv_set_double, rv_set_double_dummy),

	SM_INIT(.rv_variate_double, rv_variate_double_blksz_exp),
};

MODEXPORT int mod_config(struct module* mod) {
    self = mod;
	
	randvar_register(mod, &rv_blksz_exp_class);
	
    return MOD_OK;
}

MODEXPORT int mod_unconfig(struct module* mod) {
	randvar_unregister(mod, &rv_blksz_exp_class);
	
    return MOD_OK;
}
