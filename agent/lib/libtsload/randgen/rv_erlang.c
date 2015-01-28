#include <tsload/defs.h>

#include <tsload/mempool.h>

#include <tsload/load/randgen.h>
#include <tsload.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>


/**
 * #### Erlang distribution
 *
 * See [w:Erlang distribution](http://en.wikipedia.org/wiki/Erlang_distribution)
 *
 * Params:
 * 		* shape (int)
 * 		* rate (double)
 */

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
	 * Also, Erlang distribution doesn't uses U(0,1], so ignore zeroes.  */

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

tsload_param_t rv_erlang_params[] = {
	{ TSLOAD_PARAM_INTEGER, "shape", "must be greater than 1"},
	{ TSLOAD_PARAM_FLOAT, "rate", "cannot be negative"},
	{ TSLOAD_PARAM_NULL, NULL, NULL }
};

randvar_class_t rv_erlang_class = {
	RV_CLASS_HEAD("erlang", rv_erlang_params),
	
	SM_INIT(.rv_init, rv_init_erlang),
	SM_INIT(.rv_destroy, rv_destroy_erlang),

	SM_INIT(.rv_set_int, rv_set_int_erlang),
	SM_INIT(.rv_set_double, rv_set_double_erlang),

	SM_INIT(.rv_variate_double, rv_variate_double_erlang),
};
