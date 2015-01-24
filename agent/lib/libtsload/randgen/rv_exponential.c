
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



#include <tsload/defs.h>

#include <tsload/mempool.h>

#include <tsload/load/randgen.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>


/** #### Exponential distribution
 *
 * See [w:Exponential distribution](http://en.wikipedia.org/wiki/Exponential_distribution)
 *
 * Params:
 * 		* rate (double) */

typedef struct rv_exp {
	double rate;
} rv_exp_t;

int rv_init_exp(randvar_t* rv) {
	rv_exp_t* rve = (rv_exp_t*) mp_malloc(sizeof(rv_exp_t));

	if(rve == NULL)
		return 1;

	rve->rate = 0.0;
	rv->rv_private = rve;

	return 0;
}

void rv_destroy_exp(randvar_t* rv) {
	mp_free(rv->rv_private);
}

int rv_set_double_exp(randvar_t* rv, const char* name, double value) {
	rv_exp_t* rve = (rv_exp_t*) rv->rv_private;

	if(strcmp(name, "rate") == 0) {
		if(value <= 0.0)
			return RV_INVALID_PARAM_VALUE;

		rve->rate = value;
	}
	else {
		return RV_INVALID_PARAM_NAME;
	}

	return RV_PARAM_OK;
}

double rv_variate_double_exp(randvar_t* rv, double u) {
	rv_exp_t* rve = (rv_exp_t*) rv->rv_private;
	double x = log(1.0 - u) / -rve->rate;

	return x;
}

randvar_param_t rv_exponential_params[] = {
	{ RV_PARAM_DOUBLE, "rate", "cannot be negative" },
	{ RV_PARAM_NULL, NULL, NULL }
};

randvar_class_t rv_exponential_class = {
	RV_CLASS_HEAD("exponential", rv_exponential_params),
	
	SM_INIT(.rv_init, rv_init_exp),
	SM_INIT(.rv_destroy, rv_destroy_exp),

	SM_INIT(.rv_set_int, rv_set_int_dummy),
	SM_INIT(.rv_set_double, rv_set_double_exp),

	SM_INIT(.rv_variate_double, rv_variate_double_exp),
};

