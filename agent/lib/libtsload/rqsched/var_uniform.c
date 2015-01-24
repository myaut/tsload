#include <tsload/defs.h>

#include <tsload/mempool.h>
#include <tsload/time.h>

#include <tsload/load/randgen.h>
#include <tsload/load/rqsched.h>

#include <string.h>

int rqsvar_set_int_uniform(struct rqsched_var* var, const char* name, long value) {
	return RV_INVALID_PARAM_NAME;	
}

int rqsvar_set_double_uniform(struct rqsched_var* var, const char* name, double value) {
	if(strcmp(name, "scope") == 0) {
		if(value < 0.0 || value > 1.0)
			return RV_INVALID_PARAM_VALUE;
		
		var->params.dval = value;
		return RV_PARAM_OK;
	}
	
	return RV_INVALID_PARAM_NAME;	
}

int rqsvar_step_uniform(struct rqsched_var* var, double iat) {
	int ret;
	
	ret = rv_set_double(var->randvar, "min", (1.0 - var->params.dval) * iat);
	if(ret != RV_PARAM_OK)
		return ret;
	
	return rv_set_double(var->randvar, "max", (1.0 + var->params.dval) * iat);
}

randvar_param_t rqsvar_uniform_params[] = {
	{ RV_PARAM_DOUBLE, "scope", "should be in interval [0.0, 1.0]" },
	{ RV_PARAM_NULL, NULL, NULL }
};

rqsvar_class_t rqsvar_uniform_class = {
	AAS_CONST_STR("uniform"),
	&rv_uniform_class,
	rqsvar_uniform_params,
	
	SM_INIT(.rqsvar_init, NULL),
	SM_INIT(.rqsvar_destroy, NULL),
	
	SM_INIT(.rqsvar_set_int, rqsvar_set_int_uniform),
	SM_INIT(.rqsvar_set_double, rqsvar_set_double_uniform),
	
	SM_INIT(.rqsvar_step, rqsvar_step_uniform)
};