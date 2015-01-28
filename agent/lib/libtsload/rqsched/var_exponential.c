#include <tsload/defs.h>

#include <tsload/mempool.h>
#include <tsload/time.h>

#include <tsload/load/randgen.h>
#include <tsload/load/rqsched.h>
#include <tsload.h>

#include <string.h>

int rqsvar_set_int_exponential(struct rqsched_var* var, const char* name, long value) {
	return RV_INVALID_PARAM_NAME;	
}

int rqsvar_set_double_exponential(struct rqsched_var* var, const char* name, double value) {
	return RV_INVALID_PARAM_NAME;	
}

int rqsvar_step_exponential(struct rqsched_var* var, double iat) {
	return rv_set_double(var->randvar, "rate", 1.0 / iat);
}

tsload_param_t rqsvar_exponential_params[] = {
	RQSVAR_RANDGEN_PARAM,
	{ TSLOAD_PARAM_NULL, NULL, NULL }
};

rqsvar_class_t rqsvar_exponential_class = {
	AAS_CONST_STR("exponential"),
	&rv_exponential_class,
	rqsvar_exponential_params,
	
	SM_INIT(.rqsvar_init, NULL),
	SM_INIT(.rqsvar_destroy, NULL),
	
	SM_INIT(.rqsvar_set_int, rqsvar_set_int_exponential),
	SM_INIT(.rqsvar_set_double, rqsvar_set_double_exponential),
	
	SM_INIT(.rqsvar_step, rqsvar_step_exponential)
};