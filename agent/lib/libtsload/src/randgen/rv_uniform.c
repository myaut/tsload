#include <randgen.h>
#include <mempool.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

/**
 * #### Uniform distribution
 *
 * Params:
 * 		* min (double)
 * 		* max (double)
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
