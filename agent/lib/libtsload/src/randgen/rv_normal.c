
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

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



#include <randgen.h>
#include <mempool.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

static const double rv_normal_magic_const = 1.7155277699214135;

/**
 * #### Normal distribution
 *
 * Uses Kinderman and Monahan method
 *
 * Params:
 * 		* mean (double) - mean value
 * 		* stddev (double) - standard deviation */

typedef struct rv_normal {
	double mean;
	double stddev;
} rv_normal_t;

int rv_init_normal(randvar_t* rv) {
	rv_normal_t* rvn = (rv_normal_t*) mp_malloc(sizeof(rv_normal_t));

	if(rvn == NULL)
		return 1;

	rvn->mean = 0.0;
	rvn->stddev = 0.0;
	rv->rv_private = rvn;

	return 0;
}

void rv_destroy_normal(randvar_t* rv) {
	mp_free(rv->rv_private);
}

int rv_set_double_normal(randvar_t* rv, const char* name, double value) {
	rv_normal_t* rvn = (rv_normal_t*) rv->rv_private;

	if(strcmp(name, "mean") == 0) {
		rvn->mean = value;
	}
	else if(strcmp(name, "stddev") == 0) {
		if(value < 0.0)
			return RV_INVALID_PARAM_VALUE;

		rvn->stddev = value;
	}
	else {
		return RV_INVALID_PARAM_NAME;
	}

	return RV_PARAM_OK;
}

double rv_variate_double_normal(randvar_t* rv, double u) {
	rv_normal_t* rvn = (rv_normal_t*) rv->rv_private;
	double x;
	double u2;
	double z, zz;

	while(B_TRUE) {
		u = rg_generate_double(rv->rv_generator);
		u2 = 1 - rg_generate_double(rv->rv_generator);

		z = rv_normal_magic_const * (u - 0.5) / u2;
		zz = z * z / 4.0;

		if(zz <= -log(u2))
			break;
	}

	x = rvn->mean + z * rvn->stddev;
	return x;
}

randvar_class_t rv_normal_class = {
	SM_INIT(.rv_init, rv_init_normal),
	SM_INIT(.rv_destroy, rv_destroy_normal),

	SM_INIT(.rv_set_int, rv_set_int_dummy),
	SM_INIT(.rv_set_double, rv_set_double_normal),

	SM_INIT(.rv_variate_double, rv_variate_double_normal),
};

