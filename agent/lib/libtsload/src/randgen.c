/*
 * randgen.c
 *
 *  Created on: Nov 14, 2013
 *      Author: myaut
 */

#include <defs.h>
#include <randgen.h>
#include <mempool.h>

#include <string.h>
#include <stdlib.h>
#include <math.h>

static const double rv_normal_magic_const = 1.7155277699214135;

randgen_t* rg_create(randgen_class_t* class, uint64_t seed) {
	randgen_t* rg;

	if(class->rg_is_singleton && (class->rg_object != NULL)) {
		++class->rg_ref_count;
		return class->rg_object;
	}

	rg = (randgen_t*) mp_malloc(sizeof(randgen_t));

	if(rg == NULL)
		return NULL;

	rg->rg_class = class;
	rg->rg_seed  = seed;
	rg->rg_private = NULL;

	if(class->rg_init(rg) != 0) {
		mp_free(rg);
		return NULL;
	}

	++class->rg_ref_count;
	class->rg_object = rg;

	return rg;
}

void rg_destroy(randgen_t* rg) {
	if(--rg->rg_class->rg_ref_count == 0) {
		rg->rg_class->rg_destroy(rg);
		mp_free(rg);
	}
}

/**
 * Generates double with uniform distribution in range
 * [0.0; 1.0]
 * */
double rg_generate_double(randgen_t* rg) {
	/* By raw conversion integer to doubles we may lose precision
	 * and create bad sequences. So do the integer division and
	 * convert to double then do the double calculations:
	 *
	 * Let m - rand_max, r - random number (int), u - random number (double)
	 *
	 * x = m div r, y = m mod r (only valid ops with ints)
	 * Then:
	 *      r       1             1               1
	 * u = --- = ------ = ---------------- = -----------
	 *      m     m / r    (x * r + y) / r    x + (y / r)
	 *
	 * NOTE: Testing on Python 2.7 gives very slight difference (about 1e-16), cause double is
	 * pretty precise type. So, it's a configuration option
	 *  */
#ifndef TSLOAD_RANDGEN_FAST
	uint64_t r = rg_generate_int(rg);
	uint64_t x, y;
	double   u;

	if(r == 0)
		return 0.0;

	x = rg->rg_class->rg_max / r;
	y = rg->rg_class->rg_max % r;

	u = 1.0 / ((double)x + ((double)y / (double)r));

	return u;
#else
	return ((double) rg_generate_int(rg)) / ((double) rg->rg_class->rg_max);
#endif
}

int rg_init_dummy(randgen_t* rg) {
	return 0;
}

void rg_destroy_dummy(randgen_t* rg) {
}

/* libc (default) random generator implementation */

int rg_init_libc(randgen_t* rg) {
	srand(rg->rg_seed);

	return 0;
}

uint64_t rg_generate_int_libc(randgen_t* rg) {
	return rand();
}

randgen_class_t rg_libc = {
	RG_CLASS_HEAD(B_TRUE, RAND_MAX),

	SM_INIT(.rg_init, 		  rg_init_libc),
	SM_INIT(.rg_destroy, 	  rg_destroy_dummy),
	SM_INIT(.rg_generate_int, rg_generate_int_libc),
};

/* Variator common functions */

randvar_t* rv_create(randvar_class_t* class, randgen_t* rg) {
	randvar_t* rv = (randvar_t*) mp_malloc(sizeof(randvar_t));

	if(rv == NULL)
		return rv;

	rv->rv_class = class;
	rv->rv_generator = rg;
	rv->rv_private = NULL;

	if(class->rv_init(rv) != 0) {
		mp_free(rv);
		return NULL;
	}

	return rv;
}

void rv_destroy(randvar_t* rv) {
	rv->rv_class->rv_destroy(rv);

	mp_free(rv);
}

/* Dummy variator functions */

int rv_init_dummy(randvar_t* rg) {
	return 0;
}

void rv_destroy_dummy(randvar_t* rg) {
}

int rv_set_int_dummy(randvar_t* rv, const char* name, long value) {
	return RV_INVALID_PARAM_NAME;
}

int rv_set_double_dummy(randvar_t* rv, const char* name, double value) {
	return RV_INVALID_PARAM_NAME;
}

/* Uniformally distributed variator
 */

typedef struct rv_uniform {
	double min;
	double max;
} rv_uniform_t;

int rv_init_uniform(randvar_t* rv) {
	rv_uniform_t* rvu = (rv_uniform_t*) mp_malloc(sizeof(rv_uniform_t));

	if(rvu == NULL)
		return 1;

	rvu->min = 0.0;
	rvu->max = 1.0;
	rv->rv_private = rvu;

	return 0;
}

void rv_destroy_uniform(randvar_t* rv) {
	mp_free(rv->rv_private);
}

int rv_set_double_uniform(randvar_t* rv, const char* name, double value) {
	rv_uniform_t* rvu = (rv_uniform_t*) rv->rv_private;

	if(strcmp(name, "min") == 0) {
		rvu->min = value;
	}
	else if(strcmp(name, "max") == 0) {
		rvu->max = value;
	}
	else {
		return RV_INVALID_PARAM_NAME;
	}

	return RV_PARAM_OK;
}

double rv_variate_double_uniform(randvar_t* rv, double u) {
	rv_uniform_t* rvu = (rv_uniform_t*) rv->rv_private;

	return u * (rvu->max - rvu->min) + rvu->min;
}

randvar_class_t rv_uniform_class = {
	SM_INIT(.rv_init, rv_init_uniform),
	SM_INIT(.rv_destroy, rv_destroy_uniform),

	SM_INIT(.rv_set_int, rv_set_int_dummy),
	SM_INIT(.rv_set_double, rv_set_double_uniform),

	SM_INIT(.rv_variate_double, rv_variate_double_uniform),
};

/* Exponentially distributed variator */

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

randvar_class_t rv_exponential_class = {
	SM_INIT(.rv_init, rv_init_exp),
	SM_INIT(.rv_destroy, rv_destroy_exp),

	SM_INIT(.rv_set_int, rv_set_int_dummy),
	SM_INIT(.rv_set_double, rv_set_double_exp),

	SM_INIT(.rv_variate_double, rv_variate_double_exp),
};

/* Erlang distribution */

typedef struct rv_erlang {
	double  rate;
	long shape;
} rv_erlang_t;

int rv_init_erlang(randvar_t* rv) {
	rv_erlang_t* rve = (rv_erlang_t*) mp_malloc(sizeof(rv_erlang_t));

	if(rve == NULL)
		return 1;

	rve->shape = 0;
	rv->rv_private = rve;

	return 0;
}

void rv_destroy_erlang(randvar_t* rv) {
	mp_free(rv->rv_private);
}

int rv_set_double_erlang(randvar_t* rv, const char* name, double value) {
	rv_erlang_t* rve = (rv_erlang_t*) rv->rv_private;

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

int rv_set_int_erlang(randvar_t* rv, const char* name, long value) {
	rv_erlang_t* rve = (rv_erlang_t*) rv->rv_private;

	if(strcmp(name, "shape") == 0) {
		if(value < 1)
			return RV_INVALID_PARAM_VALUE;

		rve->shape = value;
	}
	else {
		return RV_INVALID_PARAM_NAME;
	}

	return RV_PARAM_OK;
}

double rv_variate_double_erlang(randvar_t* rv, double u) {
	rv_erlang_t* rve = (rv_erlang_t*) rv->rv_private;
	int i;
	double x;
	double m = 1.0;
	int n = rve->shape;

	/* u already generated once (in rv_variate_double), so use it on first step
	 * Also, Erlang distribution doesn't uses U(0,1], so ignore zeroes.
	 *
	 * See http://en.wikipedia.org/wiki/Erlang_distribution */

	for(i = 0; i < n; ++i) {
		if(i > 0)
			u = rg_generate_double(rv->rv_generator);

		if(likely(u > 0.0))
			m *= u;
		else
			++n;
	}

	x = log(m) / -rve->rate;
	return x;
}

randvar_class_t rv_erlang_class = {
	SM_INIT(.rv_init, rv_init_erlang),
	SM_INIT(.rv_destroy, rv_destroy_erlang),

	SM_INIT(.rv_set_int, rv_set_int_erlang),
	SM_INIT(.rv_set_double, rv_set_double_erlang),

	SM_INIT(.rv_variate_double, rv_variate_double_erlang),
};

/* Normal distribution */

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

	/* Uses Kinderman and Monahan method */
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
