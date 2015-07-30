#include <tsload/defs.h>

#include <tsload/mempool.h>
#include <tsload/time.h>

#include <tsload/load/randgen.h>
#include <tsload/load/rqsched.h>
#include <tsload.h>

#include <string.h>
#include <math.h>

int rqsvar_set_int_normal(struct rqsched_var* var, const char* name, long value) {
	return RV_INVALID_PARAM_NAME;	
}

int rqsvar_set_double_normal(struct rqsched_var* var, const char* name, double value) {
	if(strcmp(name, "covar") == 0) {
		if(value < 0.0)
			return RV_INVALID_PARAM_VALUE;
		
		var->params.dval = value;
		return RV_PARAM_OK;
	}
	
	return RV_INVALID_PARAM_NAME;	
}

int rqsvar_step_normal(struct rqsched_var* var, double iat) {
	int ret;
	
	ret = rv_set_double(var->randvar, "mean", iat);
	if(ret != RV_PARAM_OK)
		return ret;
	
	return rv_set_double(var->randvar, "stddev", var->params.dval * iat);
}

tsload_param_t rqsvar_normal_params[] = {
	RQSVAR_RANDGEN_PARAM,
	{ TSLOAD_PARAM_FLOAT, "covar", "should be positive" },
	{ TSLOAD_PARAM_NULL, NULL, NULL }
};

rqsvar_class_t rqsvar_normal_class = {
	AAS_CONST_STR("normal"),
	NULL,
	rqsvar_normal_params,
	
	SM_INIT(.rqsvar_init, NULL),
	SM_INIT(.rqsvar_destroy, NULL),
	
	SM_INIT(.rqsvar_set_int, rqsvar_set_int_normal),
	SM_INIT(.rqsvar_set_double, rqsvar_set_double_normal),
	
	SM_INIT(.rqsvar_step, rqsvar_step_normal)
};